// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/daemon.h"
#include "pgov/compiler.h"
#include "pgov/epoll.h"
#include "pgov/gov.h"
#include "pgov/lockfile.h"
#include "pgov/log.h"
#include "pgov/memory.h"
#include "pgov/node.h"
#include "pgov/opt.h"
#include "pgov/parser.h"
#include "pgov/poll.h"
#include "pgov/prop.h"
#include "pgov/psi.h"
#include "pgov/sensor.h"
#include "pgov/signal.h"
#include "pgov/state.h"
#include "pgov/task.h"
#include "pgov/thermal.h"
#include "pgov/tune.h"
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

static struct context dctx;

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

struct restore_entry {
	int fd;
	uint64_t base;
};

static struct restore_entry restore_table[NR_SINKS];
static size_t restore_count;

static void restore_snapshot(const struct context *ctx)
{
	const struct node_cache *const nodes[NR_SINKS] = {
		&ctx->sched_lat,    &ctx->sched_gran, &ctx->sched_wake,
		&ctx->sched_mig,    &ctx->sched_walt, &ctx->sched_uclmin,
		&ctx->sched_uclmax,
	};
	size_t i;

	restore_count = 0;
	for (i = 0; i < NR_SINKS; i++) {
		if (!nodes[i]->active || !nodes[i]->has_base)
			continue;

		restore_table[restore_count].fd = nodes[i]->fd;
		restore_table[restore_count].base = nodes[i]->base;
		restore_count++;
	}
}

static void crash_restore(void)
{
	char buf[32];
	size_t i;
	size_t len;
	ssize_t res;

	for (i = 0; i < restore_count; i++) {
		len = parse_fmt_u64(restore_table[i].base, buf);
		res = pwrite(restore_table[i].fd, buf, len, 0);
		UNUSED(res);
	}
}

static void restore_all(struct context *ctx)
{
	node_restore(&ctx->sched_lat);
	node_restore(&ctx->sched_gran);
	node_restore(&ctx->sched_wake);
	node_restore(&ctx->sched_mig);
	node_restore(&ctx->sched_walt);
	node_restore(&ctx->sched_uclmin);
	node_restore(&ctx->sched_uclmax);
	restore_count = 0;
}

static void init_os_environment(void)
{
	int fd;

	fd = open(PATH_LOG, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		dup2(fd, STDERR_FILENO);
		close(fd);
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

static inline void init_context_defaults(struct context *ctx)
{
	size_t i;

	ctx->shutdown_req = false;
	ctx->next_wake = POLL_MIN_MS;
	ctx->display_state = DISP_UNKNOWN;
	ctx->therm_scale = Q16_ONE;
	ctx->cpu_state.first_run = true;
	ctx->cpu_state.psi_val = 0;
	ctx->cpu_state.rate = 0;
	ctx->cpu_state.prev_integral = 0;
	ctx->efd = -1;
	ctx->sfd = -1;
	ctx->tfd = -1;
	ctx->lfd = -1;
	ctx->psi_state.fd = -1;
	ctx->cpu_temp.fd = -1;
	ctx->bat_temp.fd = -1;
	ctx->battery.fd = -1;

	ctx->backlight.count = 0;
	for (i = 0; i < SENSOR_BACKLIGHT_MAX; i++)
		ctx->backlight.fd[i] = -1;

	ctx->sched_lat.fd = -1;
	ctx->sched_gran.fd = -1;
	ctx->sched_wake.fd = -1;
	ctx->sched_mig.fd = -1;
	ctx->sched_walt.fd = -1;
	ctx->sched_uclmin.fd = -1;
	ctx->sched_uclmax.fd = -1;
	ctx->remap_pending = true;
	ctx->on_trigger = NULL;
	ctx->on_timeout = NULL;
}

static int init_node_caches(struct context *ctx)
{
	int count = 0;

	count += node_cache_init(&ctx->sched_lat, CAND_LAT);
	count += node_cache_init(&ctx->sched_gran, CAND_GRAN);
	count += node_cache_init(&ctx->sched_wake, CAND_WAKE);
	count += node_cache_init(&ctx->sched_mig, CAND_MIG);
	count += node_cache_init(&ctx->sched_walt, CAND_WALT);
	count += node_cache_init(&ctx->sched_uclmin, CAND_UCL);
	count += node_cache_init(&ctx->sched_uclmax, CAND_UCL_MAX);

	return count;
}

static int init_sensors_and_triggers(struct context *ctx)
{
	char backlight_paths[SENSOR_BACKLIGHT_MAX][SENSOR_PATH_LEN];
	char thermal_path[SENSOR_PATH_LEN];
	int count;
	int i;

	sensor_scan_thermal_zone(thermal_path, sizeof(thermal_path));
	LOGD("daemon: cpu thermal sensor mapped to %s", thermal_path);
	sensor_init_cpu_temp(&ctx->cpu_temp, thermal_path);

	sensor_init_battery_temp(&ctx->bat_temp, CAND_BAT_TEMP);
	sensor_init_battery(&ctx->battery, CAND_BAT_CAP);

	if (!ctx->dormant) {
		ctx->tfd = psi_trigger_open(PSI_CPU_PATH, PSI_THRESHOLD_US,
					    PSI_WINDOW_US);

		if (ctx->tfd < 0) {
			LOGE("daemon: failed inject err=%d", ctx->tfd);
			return ctx->tfd;
		}
	}

	psi_init(&ctx->psi_state, PSI_CPU_PATH);

	ctx->sfd = pg_signal_init();
	if (ctx->sfd < 0)
		return ctx->sfd;

	thermal_init(&ctx->therm_state);
	poll_init(&ctx->poll_state);

	count = sensor_scan_backlight(backlight_paths, SENSOR_BACKLIGHT_MAX);
	for (i = 0; i < count; i++)
		sensor_init_backlight(&ctx->backlight, backlight_paths[i]);

	return 0;
}

static void cleanup_context(struct context *ctx)
{
	restore_all(ctx);

#ifdef NDK_BUILD
	pg_opt_exit();
#endif // NDK_BUILD

	if (ctx->tfd >= 0)
		psi_trigger_close(ctx->tfd);

	if (ctx->sfd >= 0)
		pg_signal_close(ctx->sfd);

	pg_lockfile_close(ctx->lfd);
	psi_cleanup(&ctx->psi_state);
	sensor_close_temp(&ctx->cpu_temp);
	sensor_close_temp(&ctx->bat_temp);
	sensor_close_battery(&ctx->battery);
	sensor_close_backlight(&ctx->backlight);
	node_cache_cleanup(&ctx->sched_lat);
	node_cache_cleanup(&ctx->sched_gran);
	node_cache_cleanup(&ctx->sched_wake);
	node_cache_cleanup(&ctx->sched_mig);
	node_cache_cleanup(&ctx->sched_walt);
	node_cache_cleanup(&ctx->sched_uclmin);
	node_cache_cleanup(&ctx->sched_uclmax);
}

int daemon_init(void)
{
	int ret = 0;

	init_context_defaults(&dctx);
	init_os_environment();
	LOGI("daemon: initiating daemon sequence");

	dctx.lfd = pg_lockfile_init(PATH_LOCK);
	if (dctx.lfd < 0) {
		ret = dctx.lfd;
		goto cleanup;
	}

	LOGD("daemon: waiting for android subsystem");
	if (!pg_prop_wait_boot()) {
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	LOGD("daemon: mapping cache nodes");
	if (init_node_caches(&dctx) == 0) {
		LOGW("daemon: no node is writable on this kernel, gov goes dormant");
		dctx.dormant = true;
	}
	restore_snapshot(&dctx);

	if (!psi_available()) {
		ret = -ENODEV;
		goto cleanup;
	}

	LOGD("daemon: executing environment tuning");
	pg_tune_limits();
	pg_tune_configs();

	ret = init_sensors_and_triggers(&dctx);
	if (ret != 0)
		goto cleanup;

#ifdef NDK_BUILD
	pg_opt_init();
#endif // NDK_BUILD

	clock_gettime(CLOCK_MONOTONIC, &dctx.last_bat);
	clock_gettime(CLOCK_MONOTONIC, &dctx.last_therm);
	clock_gettime(CLOCK_MONOTONIC, &dctx.last_tick);

#ifdef NDK_BUILD
	clock_gettime(CLOCK_MONOTONIC, &dctx.last_sweep);
#endif // NDK_BUILD

	sensor_read_battery_capacity(&dctx.battery, &dctx.level_bat);
	sensor_read_battery_temp(&dctx.bat_temp, &dctx.temp_bat);

	dctx.on_trigger = gov_run;
	dctx.on_timeout = gov_run;
	LOGI("daemon: entering epoll reactor loop");
	ret = epoll_run(&dctx);

	LOGI("daemon: reactor shutdown cleanly status=%d", ret);

cleanup:
	cleanup_context(&dctx);
	return ret;
}
