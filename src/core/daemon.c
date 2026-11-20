// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/daemon.h"
#include "pgov/compiler.h"
#include "pgov/epoll.h"
#include "pgov/lockfile.h"
#include "pgov/opt.h"
#include "pgov/log.h"
#include "pgov/memory.h"
#include "pgov/parser.h"
#include "pgov/prop.h"
#include "pgov/signal.h"
#include "pgov/tune.h"
#include "pgov/state.h"
#include "pgov/task.h"
#include "pgov/gov.h"
#include "pgov/poll.h"
#include "pgov/psi.h"
#include "pgov/sensor.h"
#include "pgov/node.h"
#include "pgov/thermal.h"
#include <errno.h> // IWYU pragma: keep
#include <fcntl.h>
#include <unistd.h>

#define RT_PRIORITY 50
#define STACK_SIZE (512UL * 1024UL)
#define TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define IOPRIO_CLASS 2
#define IOPRIO_DATA 0

#if defined(NDK_BUILD)
#define PATH_LOCK "/data/local/tmp/pgovd.lock"
#define PATH_LOG "/data/adb/modules/pgovd/pgovd.log"
#else
#define PATH_LOCK "/data/vendor/pgovd/pgovd.lock"
#define PATH_LOG "/data/vendor/pgovd/pgovd.log"
#endif

static struct pg_context context;

static const char *const CAND_LAT[] = {
	"/proc/sys/kernel/sched_latency_ns",
	"/sys/kernel/debug/sched/latency_ns",
	NULL,
};

static const char *const CAND_GRAN[] = {
	"/proc/sys/kernel/sched_min_granularity_ns",
	"/sys/kernel/debug/sched/min_granularity_ns",
	NULL,
};

static const char *const CAND_WAKE[] = {
	"/proc/sys/kernel/sched_wakeup_granularity_ns",
	"/sys/kernel/debug/sched/wakeup_granularity_ns",
	NULL,
};

static const char *const CAND_MIG[] = {
	"/proc/sys/kernel/sched_migration_cost_ns",
	"/sys/kernel/debug/sched/migration_cost_ns",
	NULL,
};

static const char *const CAND_WALT[] = {
	"/proc/sys/kernel/sched_walt_init_task_load_pct",
	"/proc/sys/kernel/sched_init_task_load",
	NULL,
};

static const char *const CAND_UCL[] = {
	"/proc/sys/kernel/sched_uclamp_util_min",
	"/proc/sys/kernel/sched_util_clamp_min",
	NULL,
};

static const char *const CAND_UCL_MAX[] = {
	"/proc/sys/kernel/sched_uclamp_util_max",
	"/proc/sys/kernel/sched_util_clamp_max",
	NULL,
};

static const char *const CAND_BAT_TEMP[] = {
	"/sys/class/power_supply/battery/temp",
	"/sys/class/power_supply/bms/temp",
	NULL,
};

static const char *const CAND_BAT_CAP[] = {
	"/sys/class/power_supply/battery/capacity",
	"/sys/class/power_supply/bms/capacity",
	NULL,
};

#define NR_SINKS 7

static struct {
	int fd;
	uint64_t base;
} rs_tbl[NR_SINKS];

static size_t rs_cnt;

static void restore_snapshot(const struct pg_context *ctx)
{
	const struct pg_node_cache *const c[NR_SINKS] = {
		&ctx->sched_lat,     &ctx->sched_gran, &ctx->sched_wake,
		&ctx->sched_mig,     &ctx->sched_walt, &ctx->sched_ucl,
		&ctx->sched_ucl_max,
	};

	rs_cnt = 0;
	for (size_t i = 0; i < NR_SINKS; i++) {
		if (!c[i]->active || !c[i]->has_base)
			continue;

		rs_tbl[rs_cnt].fd = c[i]->fd;
		rs_tbl[rs_cnt].base = c[i]->base;
		rs_cnt++;
	}
}

static void crash_restore(void)
{
	for (size_t i = 0; i < rs_cnt; i++) {
		char buf[32];
		size_t len = pg_fmt_u64(rs_tbl[i].base, buf);
		ssize_t unused = pwrite(rs_tbl[i].fd, buf, len, 0);
		UNUSED(unused);
	}
}

static void restore_all(struct pg_context *ctx)
{
	pg_node_restore(&ctx->sched_lat);
	pg_node_restore(&ctx->sched_gran);
	pg_node_restore(&ctx->sched_wake);
	pg_node_restore(&ctx->sched_mig);
	pg_node_restore(&ctx->sched_walt);
	pg_node_restore(&ctx->sched_ucl);
	pg_node_restore(&ctx->sched_ucl_max);
	rs_cnt = 0;
}

static void init_os_environment(void)
{
	int log_fd =
		open(PATH_LOG, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);

	if (log_fd >= 0) {
		dup2(log_fd, STDERR_FILENO);
		close(log_fd);
	}

	pg_signal_catch_crash(crash_restore);
	task_protect_oom();
	pg_memory_lock();
	task_set_max_fd();
	task_set_stack_limit(STACK_SIZE);
	task_set_core_affinity();
	task_set_rt_priority(RT_PRIORITY);
	task_set_timer_slack(TIMER_SLACK_NS);
	task_set_uclamp(TASK_UCLAMP_MAX);
	task_set_io_priority(IOPRIO_CLASS, IOPRIO_DATA);
}

static inline void init_context_defaults(struct pg_context *ctx)
{
	ctx->shutdown_req = false;
	ctx->next_wake = MIN_POLL_MS;
	ctx->disp_state = DISP_UNKNOWN;
	ctx->cached_th_scl = Q16_ONE;
	ctx->load_state.first_run = true;
	ctx->load_state.psi_val = 0;
	ctx->load_state.rate = 0;
	ctx->load_state.prev_integ = 0;
	ctx->epoll_fd = -1;
	ctx->sig_fd = -1;
	ctx->trg_fd = -1;
	ctx->lock_fd = -1;
	ctx->psi.fd = -1;
	ctx->cpu_temp_sensor.fd = -1;
	ctx->bat_temp_sensor.fd = -1;
	ctx->bat_cap_sensor.fd = -1;

	ctx->bl_sensor.count = 0;
	for (size_t i = 0; i < SENSOR_BACKLIGHT_MAX; i++)
		ctx->bl_sensor.fd[i] = -1;

	ctx->sched_lat.fd = -1;
	ctx->sched_gran.fd = -1;
	ctx->sched_wake.fd = -1;
	ctx->sched_mig.fd = -1;
	ctx->sched_walt.fd = -1;
	ctx->sched_ucl.fd = -1;
	ctx->sched_ucl_max.fd = -1;
	ctx->remap_due = true;
	ctx->on_trigger = NULL;
	ctx->on_timeout = NULL;
}

static int init_node_caches(struct pg_context *ctx)
{
	int n = 0;

	n += pg_node_cache_init(&ctx->sched_lat, CAND_LAT);
	n += pg_node_cache_init(&ctx->sched_gran, CAND_GRAN);
	n += pg_node_cache_init(&ctx->sched_wake, CAND_WAKE);
	n += pg_node_cache_init(&ctx->sched_mig, CAND_MIG);
	n += pg_node_cache_init(&ctx->sched_walt, CAND_WALT);
	n += pg_node_cache_init(&ctx->sched_ucl, CAND_UCL);
	n += pg_node_cache_init(&ctx->sched_ucl_max, CAND_UCL_MAX);

	return n;
}

static int init_sensors_and_triggers(struct pg_context *ctx)
{
	char t_path[SENSOR_PATH_LEN];
	sensor_scan_thermal_zone(t_path, sizeof(t_path));
	LOGD("daemon: cpu thermal sensor mapped to %s", t_path);
	sensor_init_cpu_temp(&ctx->cpu_temp_sensor, t_path);

	sensor_init_battery_temp(&ctx->bat_temp_sensor, CAND_BAT_TEMP);
	sensor_init_battery(&ctx->bat_cap_sensor, CAND_BAT_CAP);

	if (!ctx->dormant) {
		ctx->trg_fd = psi_trigger_open(PSI_CPU_PATH, PSI_THRESHOLD_US,
					       PSI_WINDOW_US);

		if (ctx->trg_fd < 0) {
			LOGE("daemon: failed inject err=%d", ctx->trg_fd);
			return ctx->trg_fd;
		}
	}

	psi_init(&ctx->psi, PSI_CPU_PATH);

	ctx->sig_fd = pg_signal_init();
	if (ctx->sig_fd < 0)
		return ctx->sig_fd;

	pg_thermal_init(&ctx->thermal_state);
	pg_poll_init(&ctx->poll_state);

	char b_path[SENSOR_BACKLIGHT_MAX][SENSOR_PATH_LEN];
	int n_bl = sensor_scan_backlight(b_path, SENSOR_BACKLIGHT_MAX);
	for (int i = 0; i < n_bl; i++)
		sensor_init_backlight(&ctx->bl_sensor, b_path[i]);

	return 0;
}

static void cleanup_context(struct pg_context *ctx)
{
	restore_all(ctx);

#ifdef NDK_BUILD
	pg_opt_exit();
#endif // NDK_BUILD

	if (ctx->trg_fd >= 0)
		psi_trigger_close(ctx->trg_fd);

	if (ctx->sig_fd >= 0)
		pg_signal_close(ctx->sig_fd);

	pg_lockfile_close(ctx->lock_fd);
	psi_cleanup(&ctx->psi);
	sensor_close_temp(&ctx->cpu_temp_sensor);
	sensor_close_temp(&ctx->bat_temp_sensor);
	sensor_close_battery(&ctx->bat_cap_sensor);
	sensor_close_backlight(&ctx->bl_sensor);
	pg_node_cache_cleanup(&ctx->sched_lat);
	pg_node_cache_cleanup(&ctx->sched_gran);
	pg_node_cache_cleanup(&ctx->sched_wake);
	pg_node_cache_cleanup(&ctx->sched_mig);
	pg_node_cache_cleanup(&ctx->sched_walt);
	pg_node_cache_cleanup(&ctx->sched_ucl);
	pg_node_cache_cleanup(&ctx->sched_ucl_max);
}

int pg_daemon_init(void)
{
	int ret = 0;

	init_context_defaults(&context);
	init_os_environment();
	LOGI("daemon: initiating daemon sequence");

	context.lock_fd = pg_lockfile_init(PATH_LOCK);
	if (context.lock_fd < 0) {
		ret = context.lock_fd;
		goto cleanup;
	}

	LOGD("daemon: waiting for android subsystem");
	if (!pg_prop_wait_boot()) {
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	LOGD("daemon: mapping cache nodes");
	if (init_node_caches(&context) == 0) {
		LOGW("daemon: no node is writable on this kernel, gov goes dormant");
		context.dormant = true;
	}
	restore_snapshot(&context);

	if (!psi_available()) {
		ret = -ENODEV;
		goto cleanup;
	}

	LOGD("daemon: executing environment tuning");
	pg_tune_limits();
	pg_tune_configs();

	ret = init_sensors_and_triggers(&context);
	if (ret != 0)
		goto cleanup;

#ifdef NDK_BUILD
	pg_opt_init();
#endif // NDK_BUILD

	clock_gettime(CLOCK_MONOTONIC, &context.last_bat);
	clock_gettime(CLOCK_MONOTONIC, &context.last_therm);

#ifdef NDK_BUILD
	clock_gettime(CLOCK_MONOTONIC, &context.last_sweep);
#endif // NDK_BUILD

	sensor_read_battery_capacity(&context.bat_cap_sensor, &context.bat_lvl);
	sensor_read_battery_temp(&context.bat_temp_sensor, &context.bat_temp);

	pg_gov_init(&context);
	context.on_trigger = pg_gov_process;
	context.on_timeout = pg_gov_process;
	LOGI("daemon: entering epoll reactor loop");
	ret = pg_epoll_run(&context);

	LOGI("daemon: reactor shutdown cleanly status=%d", ret);

cleanup:
	cleanup_context(&context);
	return ret;
}
