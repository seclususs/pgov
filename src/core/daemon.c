// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "daemon.h"
#include "pg/config.h"
#include "detect.h"
#include "epoll.h"
#include "lockfile.h"
#include "pg/log.h"
#include "memory.h"
#include "paths.h"
#include "prop.h"
#include "rlimit.h"
#include "scan.h"
#include "pg/signal.h"
#include "pg/tune.h"
#include "pg/state.h"
#include "task.h"
#include "pg/gov.h"
#include "pg/poll.h"
#include "psi.h"
#include "sensor.h"
#include "sysfs.h"
#include "pg/thermal.h"
#include <fcntl.h>
#include <unistd.h>

static struct pg_context context;

static void init_os_environment(void)
{
	pg_signal_catch_crash();
	pg_memory_shield();
	pg_memory_lock();
	pg_rlimit_set_max_fd();
	pg_rlimit_set_stack(PG_STACK_SIZE);
	pg_task_set_core_efficiency();
	pg_task_set_rt_prio(PG_RT_PRIORITY);
	pg_task_set_timer_slack(PG_TIMER_SLACK_NS);
	pg_task_set_uclamp(PG_UCLAMP_MAX);
	pg_task_set_io_prio(PG_IOPRIO_CLASS, PG_IOPRIO_DATA);
}

static inline void init_context_defaults(struct pg_context *ctx)
{
	ctx->shutdown_req = false;
	ctx->next_wake = PG_MIN_POLL_MS;
	ctx->disp_state = PG_DISP_UNKNOWN;
	ctx->load_state.first_run = true;
	ctx->load_state.psi_val = 0;
	ctx->load_state.rate = 0;
	ctx->load_state.prev_integ = 0;
	ctx->epoll_fd = -1;
	ctx->sig_fd = -1;
	ctx->trg_fd = -1;
	ctx->psi.fd = -1;
	ctx->cpu_temp_sensor.fd = -1;
	ctx->bat_temp_sensor.fd = -1;
	ctx->bat_cap_sensor.fd = -1;
	ctx->bl_sensor.fd = -1;
	ctx->on_trigger = NULL;
	ctx->on_timeout = NULL;
}

static void init_sysfs_caches(struct pg_context *ctx)
{
	pg_sysfs_cache_init(&ctx->sched_lat, PG_PATH_SCHED_LAT);
	pg_sysfs_cache_init(&ctx->sched_gran, PG_PATH_SCHED_GRAN);
	pg_sysfs_cache_init(&ctx->sched_wake, PG_PATH_SCHED_WAKEUP);
	pg_sysfs_cache_init(&ctx->sched_mig, PG_PATH_SCHED_MIG);
	pg_sysfs_cache_init(&ctx->sched_walt, PG_PATH_SCHED_WALT);
	pg_sysfs_cache_init(&ctx->sched_ucl, PG_PATH_SCHED_UCLAMP);
}

static int init_sensors_and_triggers(struct pg_context *ctx)
{
	char t_path[256];
	pg_scan_thermal_zone(t_path, sizeof(t_path));
	LOGD("daemon: cpu thermal sensor mapped to %s", t_path);
	pg_sensor_init_cpu_temp(&ctx->cpu_temp_sensor, t_path);

	pg_sensor_init_bat_temp(&ctx->bat_temp_sensor, PG_PATH_BATTERY_TEMP);
	pg_sensor_init_bat_cap(&ctx->bat_cap_sensor, PG_PATH_BATTERY_CAP);

	ctx->trg_fd = pg_psi_open_trg(PG_PATH_PSI_CPU, PG_CFG_CTRL.thresh_us,
				      PG_CFG_CTRL.win_us);

	if (ctx->trg_fd < 0) {
		LOGE("daemon: failed inject err=%d", ctx->trg_fd);
		return -1;
	}

	pg_psi_init(&ctx->psi, PG_PATH_PSI_CPU);

	ctx->sig_fd = pg_signal_init();
	if (ctx->sig_fd < 0)
		return -1;

	pg_thermal_init(&ctx->thermal_state);
	pg_poll_init(&ctx->poll_state);

	char b_path[256];
	pg_scan_backlight(b_path, sizeof(b_path));
	pg_sensor_init_bl(&ctx->bl_sensor, b_path);

	return 0;
}

int pg_daemon_init(void)
{
	int ret = 0;

	LOGI("daemon: initiating daemon sequence");

	if (pg_detect_privilege() != 0)
		return 1;

	if (pg_lockfile_acquire(PG_PATH_LOCK) != 0)
		return 1;

	LOGD("daemon: waiting for android subsystem");
	pg_prop_wait_boot();

	LOGD("daemon: configuring os parameters");
	init_os_environment();

	if (!pg_detect_cpu_psi())
		return 1;

	LOGD("daemon: initializing state context");
	init_context_defaults(&context);
	init_sysfs_caches(&context);

	LOGD("daemon: executing limits tuning");
	pg_tune_limits();

	if (init_sensors_and_triggers(&context) != 0) {
		ret = 1;
		goto cleanup;
	}

	clock_gettime(CLOCK_MONOTONIC, &context.last_bat);

#if defined(NDK_BUILD)
	clock_gettime(CLOCK_MONOTONIC, &context.last_sweep);
#endif // NDK_BUILD

	pg_sensor_read_bat_cap(&context.bat_cap_sensor, &context.bat_lvl);
	pg_sensor_read_bat_temp(&context.bat_temp_sensor, &context.bat_temp);

	pg_gov_init(&context);
	context.on_trigger = pg_gov_process;
	context.on_timeout = pg_gov_process;
	LOGI("daemon: entering epoll reactor loop");
	ret = pg_epoll_run(&context);

	LOGI("daemon: reactor shutdown cleanly status=%d", ret);

cleanup:
	if (context.trg_fd >= 0)
		pg_psi_close_trg(context.trg_fd);

	if (context.sig_fd >= 0)
		pg_signal_close(context.sig_fd);

	pg_psi_cleanup(&context.psi);
	pg_sensor_temp_close(&context.cpu_temp_sensor);
	pg_sensor_temp_close(&context.bat_temp_sensor);
	pg_sensor_bat_close(&context.bat_cap_sensor);
	pg_sensor_bl_close(&context.bl_sensor);
	pg_sysfs_cache_cleanup(&context.sched_lat);
	pg_sysfs_cache_cleanup(&context.sched_gran);
	pg_sysfs_cache_cleanup(&context.sched_wake);
	pg_sysfs_cache_cleanup(&context.sched_mig);
	pg_sysfs_cache_cleanup(&context.sched_walt);
	pg_sysfs_cache_cleanup(&context.sched_ucl);

	return ret;
}
