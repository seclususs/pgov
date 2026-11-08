// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/daemon.h"
#include "pgov/config.h"
#include "pgov/compiler.h"
#include "pgov/detect.h"
#include "pgov/epoll.h"
#include "pgov/lockfile.h"
#include "pgov/opt.h"
#include "pgov/log.h"
#include "pgov/memory.h"
#include "pgov/parser.h"
#include "pgov/path.h"
#include "pgov/prop.h"
#include "pgov/rlimit.h"
#include "pgov/scan.h"
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

static struct pg_context context;

static const char *const CAND_LAT[] = {
	PATH_SCHED_LAT,
	PATH_SCHED_LAT_DBG,
	NULL,
};

static const char *const CAND_GRAN[] = {
	PATH_SCHED_GRAN,
	PATH_SCHED_GRAN_DBG,
	NULL,
};

static const char *const CAND_WAKE[] = {
	PATH_SCHED_WAKEUP,
	PATH_SCHED_WAKEUP_DBG,
	NULL,
};

static const char *const CAND_MIG[] = {
	PATH_SCHED_MIG,
	PATH_SCHED_MIG_DBG,
	NULL,
};

static const char *const CAND_WALT[] = {
	PATH_SCHED_WALT,
	NULL,
};

static const char *const CAND_UCL[] = {
	PATH_SCHED_UCLAMP_UTIL,
	PATH_SCHED_UTIL_CLAMP,
	NULL,
};

#define NR_SINKS 6

static struct {
	int fd;
	uint64_t base;
} rs_tbl[NR_SINKS];

static size_t rs_cnt;

static void restore_snapshot(const struct pg_context *ctx)
{
	const struct pg_node_cache *const c[NR_SINKS] = {
		&ctx->sched_lat, &ctx->sched_gran, &ctx->sched_wake,
		&ctx->sched_mig, &ctx->sched_walt, &ctx->sched_ucl,
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
	pg_memory_shield();
	pg_memory_lock();
	pg_rlimit_set_max_fd();
	pg_rlimit_set_stack(CFG_STACK_SIZE);
	pg_task_set_core_efficiency();
	pg_task_set_rt_prio(CFG_RT_PRIORITY);
	pg_task_set_timer_slack(CFG_TIMER_SLACK_NS);
	pg_task_set_uclamp(CFG_UCLAMP_MAX);
	pg_task_set_io_prio(CFG_IOPRIO_CLASS, CFG_IOPRIO_DATA);
}

static inline void init_context_defaults(struct pg_context *ctx)
{
	ctx->shutdown_req = false;
	ctx->next_wake = CFG_MIN_POLL_MS;
	ctx->disp_state = PG_DISP_UNKNOWN;
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

	ctx->bl_sensor.n = 0;
	for (size_t i = 0; i < CFG_BL_MAX; i++)
		ctx->bl_sensor.fd[i] = -1;

	ctx->sched_lat.fd = -1;
	ctx->sched_gran.fd = -1;
	ctx->sched_wake.fd = -1;
	ctx->sched_mig.fd = -1;
	ctx->sched_walt.fd = -1;
	ctx->sched_ucl.fd = -1;
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

	return n;
}

static int init_sensors_and_triggers(struct pg_context *ctx)
{
	char t_path[256];
	pg_scan_thermal_zone(t_path, sizeof(t_path));
	LOGD("daemon: cpu thermal sensor mapped to %s", t_path);
	pg_sensor_init_cpu_temp(&ctx->cpu_temp_sensor, t_path);

	pg_sensor_init_bat_temp(&ctx->bat_temp_sensor, PATH_BATTERY_TEMP);
	pg_sensor_init_bat_cap(&ctx->bat_cap_sensor, PATH_BATTERY_CAP);

	if (!ctx->dormant) {
		ctx->trg_fd = pg_psi_open_trg(
			PATH_PSI_CPU, CFG_PSI_THRESHOLD_US, CFG_PSI_WINDOW_US);

		if (ctx->trg_fd < 0) {
			LOGE("daemon: failed inject err=%d", ctx->trg_fd);
			return ctx->trg_fd;
		}
	}

	pg_psi_init(&ctx->psi, PATH_PSI_CPU);

	ctx->sig_fd = pg_signal_init();
	if (ctx->sig_fd < 0)
		return ctx->sig_fd;

	pg_thermal_init(&ctx->thermal_state);
	pg_poll_init(&ctx->poll_state);

	char b_path[CFG_BL_MAX][PG_BL_PATH_LEN];
	int n_bl = pg_scan_backlight(b_path, CFG_BL_MAX);
	for (int i = 0; i < n_bl; i++)
		pg_sensor_init_bl(&ctx->bl_sensor, b_path[i]);

	return 0;
}

static void cleanup_context(struct pg_context *ctx)
{
	restore_all(ctx);

#ifdef NDK_BUILD
	pg_opt_exit();
#endif // NDK_BUILD

	if (ctx->trg_fd >= 0)
		pg_psi_close_trg(ctx->trg_fd);

	if (ctx->sig_fd >= 0)
		pg_signal_close(ctx->sig_fd);

	pg_lockfile_close(ctx->lock_fd);
	pg_psi_cleanup(&ctx->psi);
	pg_sensor_temp_close(&ctx->cpu_temp_sensor);
	pg_sensor_temp_close(&ctx->bat_temp_sensor);
	pg_sensor_bat_close(&ctx->bat_cap_sensor);
	pg_sensor_bl_close(&ctx->bl_sensor);
	pg_node_cache_cleanup(&ctx->sched_lat);
	pg_node_cache_cleanup(&ctx->sched_gran);
	pg_node_cache_cleanup(&ctx->sched_wake);
	pg_node_cache_cleanup(&ctx->sched_mig);
	pg_node_cache_cleanup(&ctx->sched_walt);
	pg_node_cache_cleanup(&ctx->sched_ucl);
}

int pg_daemon_init(void)
{
	int ret = 0;

	LOGI("daemon: initiating daemon sequence");
	init_context_defaults(&context);

	ret = pg_detect_privilege();
	if (ret != 0)
		goto cleanup;

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

	LOGD("daemon: configuring os parameters");
	init_os_environment();

	if (!pg_detect_cpu_psi()) {
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

	pg_sensor_read_bat_cap(&context.bat_cap_sensor, &context.bat_lvl);
	pg_sensor_read_bat_temp(&context.bat_temp_sensor, &context.bat_temp);

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
