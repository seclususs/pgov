// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "daemon/daemon.h"
#include "daemon/config.h"
#include "daemon/detect.h"
#include "daemon/epoll.h"
#include "daemon/lockfile.h"
#include "daemon/logger.h"
#include "daemon/memory.h"
#include "daemon/paths.h"
#include "daemon/properties.h"
#include "daemon/rlimit.h"
#include "daemon/scan.h"
#include "daemon/signal.h"
#include "daemon/state.h"
#include "daemon/task.h"
#include "pascal_gov/governor.h"
#include "pascal_gov/poller.h"
#include "pascal_gov/psi.h"
#include "pascal_gov/sensor.h"
#include "pascal_gov/sysfs.h"
#include "pascal_gov/thermal.h"
#include <fcntl.h>
#include <unistd.h>

static pascal_gov_context context;

static void init_os_environment(void)
{
	pascal_gov_signal_catch_crashes();
	pascal_gov_memory_shield();
	pascal_gov_memory_lock();
	pascal_gov_rlimit_set_max_fd();
	pascal_gov_rlimit_set_stack(PASCAL_GOV_STACK_SIZE);
	pascal_gov_task_enforce_efficiency();
	pascal_gov_task_set_realtime(PASCAL_GOV_RT_PRIORITY);
	pascal_gov_task_maximize_timer_slack(PASCAL_GOV_TIMER_SLACK_NS);
	pascal_gov_task_limit_uclamp(PASCAL_GOV_UCLAMP_MAX);
	pascal_gov_task_set_io_priority(PASCAL_GOV_IOPRIO_CLASS,
					PASCAL_GOV_IOPRIO_DATA);
}

static void init_sysfs_caches(pascal_gov_context *ctx)
{
	pascal_gov_sysfs_cache_init(&ctx->sched_latency,
				    PASCAL_GOV_PATH_SCHED_LATENCY_NS);

	pascal_gov_sysfs_cache_init(&ctx->sched_min_granularity,
				    PASCAL_GOV_PATH_SCHED_MIN_GRANULARITY_NS);

	pascal_gov_sysfs_cache_init(
		&ctx->sched_wakeup_granularity,
		PASCAL_GOV_PATH_SCHED_WAKEUP_GRANULARITY_NS);

	pascal_gov_sysfs_cache_init(&ctx->sched_migration_cost,
				    PASCAL_GOV_PATH_SCHED_MIGRATION_COST_NS);

	pascal_gov_sysfs_cache_init(&ctx->sched_walt_init,
				    PASCAL_GOV_PATH_SCHED_WALT_INIT);

	pascal_gov_sysfs_cache_init(&ctx->sched_uclamp_min,
				    PASCAL_GOV_PATH_SCHED_UCLAMP_MIN);
}

static int init_sensors_and_triggers(pascal_gov_context *ctx)
{
	char cpu_thermal_path[256];
	pascal_gov_scan_thermal_zone(cpu_thermal_path,
				     sizeof(cpu_thermal_path));

	LOGD("daemon: cpu thermal sensor mapped to %s", cpu_thermal_path);

	pascal_gov_sensor_init_cpu_thermal(&ctx->cpu_temp_sensor,
					   cpu_thermal_path);

	pascal_gov_sensor_init_bat_thermal(&ctx->bat_temp_sensor,
					   PASCAL_GOV_PATH_BATTERY_TEMP);

	pascal_gov_sensor_init_battery(&ctx->bat_capacity_sensor,
				       PASCAL_GOV_PATH_BATTERY_CAPACITY);

	ctx->trigger_fd = pascal_gov_psi_register_trigger(
		PASCAL_GOV_PATH_PSI_CPU,
		PASCAL_GOV_CONFIG_CONTROLLER.psi_threshold_us,
		PASCAL_GOV_CONFIG_CONTROLLER.psi_window_us);

	if (ctx->trigger_fd < 0) {
		LOGE("daemon: failed inject err=%d", ctx->trigger_fd);
		return -1;
	}

	pascal_gov_psi_init(&ctx->psi_monitor, PASCAL_GOV_PATH_PSI_CPU,
			    &PASCAL_GOV_CONFIG_KALMAN);

	ctx->signal_fd = pascal_gov_signal_init();
	if (ctx->signal_fd < 0) {
		return -1;
	}

	pascal_gov_thermal_init(&ctx->thermal_state);

	pascal_gov_poller_init(
		&ctx->poller_state,
		PASCAL_GOV_CONFIG_CONTROLLER.poll_weight_pressure,
		PASCAL_GOV_CONFIG_CONTROLLER.poll_weight_derivative,
		&PASCAL_GOV_CONFIG_POLLER);

	return 0;
}

int pascal_gov_daemon_init(void)
{
	int status = 0;

	LOGI("daemon: initiating daemon sequence");

	if (pascal_gov_detect_privilege() != 0)
		return 1;

	if (pascal_gov_lockfile_acquire(PASCAL_GOV_LOCK_PATH) != 0)
		return 1;

	LOGD("daemon: waiting for android subsystem");
	pascal_gov_properties_wait_boot();

	LOGD("daemon: configuring os parameters");
	init_os_environment();

	if (!pascal_gov_detect_cpu_psi()) {
		LOGE("daemon: abort, cpu psi node is strictly required");
		return 1;
	}

	LOGD("daemon: initializing state context");
	context.shutdown_requested = false;
	context.next_wake_ms = PASCAL_GOV_MIN_POLLING_MS;
	context.cpu_load_state.first_run = true;
	context.cpu_load_state.psi_value = 0.0F;
	context.cpu_load_state.rate = 0.0F;
	context.cpu_load_state.prev_integral = 0.0F;
	context.epoll_fd = -1;
	context.signal_fd = -1;
	context.trigger_fd = -1;
	context.psi_monitor.fd = -1;
	context.cpu_temp_sensor.fd = -1;
	context.bat_temp_sensor.fd = -1;
	context.bat_capacity_sensor.fd = -1;
	context.on_trigger = NULL;
	context.on_timeout = NULL;

	init_sysfs_caches(&context);

	if (init_sensors_and_triggers(&context) != 0) {
		status = 1;
		goto cleanup;
	}

	clock_gettime(CLOCK_MONOTONIC, &context.last_battery_check);

	pascal_gov_sensor_read_battery(&context.bat_capacity_sensor,
				       &context.cached_battery_level);

	pascal_gov_sensor_read_bat_temp(&context.bat_temp_sensor,
					&context.cached_battery_temp);

	pascal_gov_governor_init(&context);

	context.on_trigger = pascal_gov_governor_process_cpu;
	context.on_timeout = pascal_gov_governor_process_cpu;

	LOGI("daemon: entering epoll reactor loop");
	status = pascal_gov_epoll_run(&context);

	LOGI("daemon: reactor shutdown cleanly status=%d", status);

cleanup:
	if (context.trigger_fd >= 0)
		pascal_gov_psi_unregister_trigger(context.trigger_fd);

	if (context.signal_fd >= 0)
		pascal_gov_signal_destroy(context.signal_fd);

	pascal_gov_psi_destroy(&context.psi_monitor);
	pascal_gov_sensor_destroy_thermal(&context.cpu_temp_sensor);
	pascal_gov_sensor_destroy_thermal(&context.bat_temp_sensor);
	pascal_gov_sensor_destroy_battery(&context.bat_capacity_sensor);

	pascal_gov_sysfs_cache_destroy(&context.sched_latency);
	pascal_gov_sysfs_cache_destroy(&context.sched_min_granularity);
	pascal_gov_sysfs_cache_destroy(&context.sched_wakeup_granularity);
	pascal_gov_sysfs_cache_destroy(&context.sched_migration_cost);
	pascal_gov_sysfs_cache_destroy(&context.sched_walt_init);
	pascal_gov_sysfs_cache_destroy(&context.sched_uclamp_min);

	return status;
}
