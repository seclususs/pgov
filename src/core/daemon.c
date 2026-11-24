// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/daemon.h"
#include "pgov/epoll.h"
#include "pgov/gov.h"
#include "pgov/lockfile.h"
#include "pgov/log.h"
#include "pgov/memory.h"
#include "pgov/opt.h"
#include "pgov/poll.h"
#include "pgov/prop.h"
#include "pgov/psi.h"
#include "pgov/sched.h"
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

static void init_os_environment(void)
{
	int fd;

	fd = open(PATH_LOG, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
	if (fd >= 0) {
		dup2(fd, STDERR_FILENO);
		close(fd);
	}

	signal_catch_crash(sched_restore);
	task_protect_oom();
	memory_lock();
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
	ctx->sched_state.first_run = true;
	ctx->sched_state.psi_val = 0;
	ctx->sched_state.rate = 0;
	ctx->sched_state.prev_integral = 0;
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

static int init_sensors_and_triggers(struct context *ctx)
{
	char backlight_paths[SENSOR_BACKLIGHT_MAX][SENSOR_PATH_LEN];
	char thermal_path[SENSOR_PATH_LEN];
	int count;
	int i;

	sensor_scan_thermal_zone(thermal_path, sizeof(thermal_path));
	sensor_init_cpu_temp(&ctx->cpu_temp, thermal_path);

	sensor_init_battery_temp(&ctx->bat_temp, CAND_BAT_TEMP);
	sensor_init_battery(&ctx->battery, CAND_BAT_CAP);

	if (!ctx->dormant) {
		ctx->tfd = psi_open(PSI_CPU, PSI_THRESHOLD, PSI_WINDOW);
		if (ctx->tfd < 0)
			return ctx->tfd;
	}

	psi_init(&ctx->psi_state, PSI_CPU);

	ctx->sfd = signal_init();
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
	sched_cleanup(ctx);

#ifdef NDK_BUILD
	opt_exit();
#endif // NDK_BUILD

	if (ctx->tfd >= 0)
		psi_close(ctx->tfd);

	if (ctx->sfd >= 0)
		signal_close(ctx->sfd);

	lockfile_close(ctx->lfd);
	psi_cleanup(&ctx->psi_state);
	sensor_close_temp(&ctx->cpu_temp);
	sensor_close_temp(&ctx->bat_temp);
	sensor_close_battery(&ctx->battery);
	sensor_close_backlight(&ctx->backlight);
}

int daemon_init(void)
{
	int ret = 0;

	init_context_defaults(&dctx);

	dctx.lfd = lockfile_init(PATH_LOCK);
	if (dctx.lfd < 0)
		return dctx.lfd;

	init_os_environment();
	LOGI("daemon: initiating");

	if (!prop_wait_boot()) {
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	if (sched_init(&dctx) == 0)
		dctx.dormant = true;

	sched_snapshot(&dctx);

	if (!psi_available()) {
		ret = -ENODEV;
		goto cleanup;
	}

	tune_limits();
	tune_params();

	ret = init_sensors_and_triggers(&dctx);
	if (ret != 0)
		goto cleanup;

#ifdef NDK_BUILD
	opt_init();
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
	LOGI("daemon: running mode=%s", dctx.dormant ? "dormant" : "active");
	ret = epoll_run(&dctx);

	LOGI("daemon: shutdown status=%d", ret);

cleanup:
	if (ret != 0)
		LOGE("daemon: aborted status=%d", ret);

	cleanup_context(&dctx);
	return ret;
}
