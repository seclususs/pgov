// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/gov.h"
#include "pgov/compiler.h"
#include "pgov/cpu.h"
#include "pgov/epoll.h"
#include "pgov/log.h"
#include "pgov/math.h"
#include "pgov/node.h"
#include "pgov/param.h"
#include "pgov/poll.h"
#include "pgov/psi.h"
#include "pgov/sensor.h"
#include "pgov/sweep.h"
#include "pgov/thermal.h"
#include "pgov/time.h"
#include <unistd.h>

#define QUANT_NS 65536ULL
#define CHECK_LAT 100
#define CHECK_GRAN 100
#define CHECK_WAKE 150
#define CHECK_MIG 50000
#define CHECK_WALT 5
#define CHECK_UCL 32
#define BAT_CHECK_SEC 5
#define THERM_CHECK_SEC 1
#define DISP_CHECK_SEC 1
#define TEMP_FAIL_MAX 10
#define PSI_FAIL_MAX 6

#if defined(NDK_BUILD)
#define SWEEP_IVL_SEC 600
#endif

static const struct node_check check_lat = {
	NODE_CHECK_RELATIVE,
	CHECK_LAT,
};

static const struct node_check check_gran = {
	NODE_CHECK_RELATIVE,
	CHECK_GRAN,
};

static const struct node_check check_wake = {
	NODE_CHECK_RELATIVE,
	CHECK_WAKE,
};

static const struct node_check check_mig = {
	NODE_CHECK_ABSOLUTE,
	CHECK_MIG,
};

static const struct node_check check_walt = {
	NODE_CHECK_ABSOLUTE,
	CHECK_WALT,
};

static const struct node_check check_ucl = {
	NODE_CHECK_ABSOLUTE,
	CHECK_UCL,
};

static inline void relax_sched_tunables(struct context *ctx)
{
	const uint64_t quant = QUANT_NS;
	uint64_t val;

	if (ctx->sched_uclmin.active && ctx->sched_uclmin.last != 0)
		node_update(&ctx->sched_uclmin, 0, true, NULL);

	if (ctx->sched_uclmax.active) {
		val = math_sanitize_u64(
			param_sched.susp_ucl,
			(uint64_t)Q16_TO_INT(param_sched.susp_ucl));

		if (ctx->sched_uclmax.last != val)
			node_update(&ctx->sched_uclmax, val, true, NULL);
	}

	if (ctx->sched_lat.active) {
		val = math_quantize_u64(param_sched.max_lat,
					param_sched.max_lat, quant);

		if (ctx->sched_lat.last != val)
			node_update(&ctx->sched_lat, val, true, NULL);
	}

	if (ctx->sched_gran.active) {
		val = math_quantize_u64(param_sched.max_gran,
					param_sched.max_gran, quant);

		if (ctx->sched_gran.last != val)
			node_update(&ctx->sched_gran, val, true, NULL);
	}

	if (ctx->sched_wake.active) {
		val = math_quantize_u64(param_sched.max_wake,
					param_sched.max_wake, quant);

		if (ctx->sched_wake.last != val)
			node_update(&ctx->sched_wake, val, true, NULL);
	}

	if (ctx->sched_mig.active) {
		val = math_quantize_u64(param_sched.max_mig,
					param_sched.min_mig, quant);

		if (ctx->sched_mig.last != val)
			node_update(&ctx->sched_mig, val, true, NULL);
	}

	if (ctx->sched_walt.active) {
		val = math_sanitize_u64(
			param_sched.min_walt,
			(uint64_t)Q16_TO_INT(param_sched.min_walt));

		if (ctx->sched_walt.last != val)
			node_update(&ctx->sched_walt, val, true, NULL);
	}
}

static inline bool update_display(struct context *ctx,
				  const struct timespec *now)
{
	int32_t brightness = 0;
	bool on;
	int ret;

	if (LIKELY(ctx->display_state == DISP_ON) &&
	    time_dt_sec(&ctx->last_disp, now) < INT_TO_Q16(DISP_CHECK_SEC))
		return false;

	ctx->last_disp = *now;

	ret = sensor_read_backlight(&ctx->backlight, &brightness);
	on = ((ret < 0) ? true : (brightness > 0)) != 0;

	if (on && UNLIKELY(ctx->display_state != DISP_ON)) {
		ctx->cpu_state.first_run = true;
		kalman_reset(&ctx->psi_state.filter);
		ctx->psi_state.initial = true;
		ctx->display_state = DISP_ON;
		ctx->remap_pending = true;

	} else if (!on && (ctx->display_state == DISP_ON ||
			   ctx->display_state == DISP_UNKNOWN)) {
		ctx->display_state = DISP_GRACE;
		ctx->last_off = *now;

	} else if (!on && ctx->display_state == DISP_GRACE) {
		if (time_dt_sec(&ctx->last_off, now) > INT_TO_Q16(10))
			ctx->display_state = DISP_SUSPEND;
	}

	if (ctx->display_state == DISP_SUSPEND) {
		relax_sched_tunables(ctx);

		ctx->next_wake = 5000;
		ctx->last_tick = *now;

#ifdef NDK_BUILD
		q16_t elapsed;

		elapsed = time_dt_sec(&ctx->last_sweep, now);
		if (elapsed > INT_TO_Q16(SWEEP_IVL_SEC)) {
			sweep_run(ctx);
			clock_gettime(CLOCK_MONOTONIC, &ctx->last_sweep);
		}
#endif // NDK_BUILD

		return true;
	}

	return false;
}

static void register_psi_trigger(struct context *ctx)
{
	int fd;

	fd = psi_trigger_open(PSI_CPU_PATH, PSI_THRESHOLD_US, PSI_WINDOW_US);
	if (fd < 0)
		return;

	ctx->tfd = fd;
	if (epoll_add_trigger(ctx) < 0) {
		psi_trigger_close(fd);
		ctx->tfd = -1;
	}
}

static inline bool read_psi(struct context *ctx, const struct timespec *now,
			    struct psi_sample *sample)
{
	uint32_t delay;
	int ret;

	ret = psi_read(&ctx->psi_state, sample, now);
	if (LIKELY(ret == 0)) {
		if (UNLIKELY(ctx->psi_fails != 0)) {
			ctx->psi_fails = 0;
			LOGI("gov: psi read recovered");
		}

		return true;
	}

	if (ctx->psi_fails < PSI_FAIL_MAX) {
		ctx->psi_fails++;
		LOGW("gov: psi read failed status=%d, init recov", ret);
	}

	delay = 1000U << (ctx->psi_fails - 1);
	ctx->next_wake = (int)(delay > 30000U ? 30000U : delay);

	epoll_rm_trigger(ctx);

	if (ctx->tfd >= 0) {
		psi_trigger_close(ctx->tfd);
		ctx->tfd = -1;
	}

	psi_recover(&ctx->psi_state, PSI_CPU_PATH);
	register_psi_trigger(ctx);

	return false;
}

static inline void poll_battery_metrics(struct context *ctx,
					const struct timespec *now)
{
	q16_t elapsed;

	elapsed = time_dt_sec(&ctx->last_bat, now);
	if (elapsed < INT_TO_Q16(BAT_CHECK_SEC))
		return;

	sensor_read_battery_capacity(&ctx->battery, &ctx->level_bat);
	sensor_read_battery_temp(&ctx->bat_temp, &ctx->temp_bat);
	ctx->last_bat = *now;

	if (UNLIKELY(ctx->tfd < 0))
		register_psi_trigger(ctx);
}

static inline void poll_thermal_metrics(struct context *ctx,
					const struct timespec *now)
{
	q16_t cpu_temp = 0;
	q16_t elapsed;
	q16_t scale;
	bool hold = false;

	elapsed = time_dt_sec(&ctx->last_therm, now);
	if (elapsed < INT_TO_Q16(THERM_CHECK_SEC))
		return;

	if (LIKELY(ctx->cpu_temp.fd >= 0)) {
		if (sensor_read_cpu_temp(&ctx->cpu_temp, &cpu_temp) == 0) {
			ctx->temp_fails = 0;

		} else if (ctx->temp_fails < TEMP_FAIL_MAX) {
			ctx->temp_fails++;
			hold = true;

		} else {
			cpu_temp = 0;
		}
	}

	if (LIKELY(!hold)) {
		scale = thermal_update(&ctx->therm_state, cpu_temp,
				       ctx->temp_bat, &param_thermal, now);
		ctx->therm_scale = scale;
	}

	ctx->last_therm = *now;
	ctx->remap_pending = true;
}

static inline void calc_demand(struct context *ctx, const struct timespec *now,
			       const struct psi_sample *sample,
			       q16_t *RESTRICT therm_scale,
			       q16_t *RESTRICT demand, q16_t *RESTRICT pressure)
{
	struct cpu_demand input;
	struct cpu_efficiency *eff;
	q16_t dt_real;
	q16_t dt_safe;
	q16_t integral;
	q16_t integral_dt;
	q16_t trend_factor;
	bool struct_break;

	poll_battery_metrics(ctx, now);
	poll_thermal_metrics(ctx, now);

	*therm_scale = ctx->therm_scale;

	eff = &ctx->cpu_state.efficiency;
	cpu_update_efficiency(eff, ctx->level_bat, *therm_scale,
			      sample->some.avg300);

	dt_real = time_dt_sec(&ctx->last_tick, now);
	dt_safe = math_clamp(dt_real, 1, FLOAT_TO_Q16(0.1F));
	ctx->last_tick = *now;

	cpu_update_integral(&ctx->cpu_state, ctx->level_bat, dt_real, &integral,
			    &integral_dt);

	trend_factor = cpu_calc_trend_gain(sample->some.velocity);
	struct_break = sample->some.nis > eff->nis_thresh;

	input.target_psi = sample->some.stall_rate;
	input.velocity = sample->some.velocity;
	input.dt_safe = dt_safe;
	input.therm_scale = *therm_scale;
	input.trend_factor = trend_factor;
	input.integral = integral;
	input.integral_dt = integral_dt;
	input.struct_break = struct_break;

	*demand = cpu_calc_load_demand(&ctx->cpu_state, &input, &param_cpu);
	*pressure = cpu_calc_effective_pressure(*demand, trend_factor, eff);
}

static inline bool should_apply_mapping(struct context *ctx,
					const struct psi_sample *sample,
					q16_t pressure)
{
	int32_t next_wake;
	int32_t trans_wake;
	bool moving;

	next_wake = (int32_t)poll_calc_next(&ctx->poll_state, pressure,
					    sample->some.avg300,
					    sample->some.velocity);
	ctx->next_wake = next_wake;

	moving = cpu_transient(&ctx->cpu_state, &param_cpu);
	if (!moving && !ctx->remap_pending)
		return false;

	if (moving) {
		trans_wake = Q16_TO_INT(param_cpu.trans_poll);
		if (next_wake > trans_wake)
			ctx->next_wake = trans_wake;
	}

	ctx->cpu_state.psi_ref = ctx->cpu_state.psi_val;
	ctx->remap_pending = false;
	return true;
}

static inline void apply_mapping(struct context *ctx,
				 const struct psi_sample *sample,
				 q16_t therm_scale, q16_t demand,
				 q16_t pressure)
{
	const uint64_t quant = QUANT_NS;
	struct cpu_efficiency *eff = &ctx->cpu_state.efficiency;
	uint64_t out_gran = 0;
	uint64_t out_lat = 0;
	uint64_t out_mig = 0;
	uint64_t out_uclmax = CPU_UCLAMP_MAX_CEILING;
	uint64_t out_uclmin = 0;
	uint64_t out_wake = 0;
	uint64_t out_walt = 0;
	q16_t gran = 0;
	q16_t lat = 0;
	q16_t mig = 0;
	q16_t therm_lat = 0;
	q16_t uclmax = 0;
	q16_t uclmin = 0;
	q16_t wake = 0;
	q16_t walt = 0;

	if (ctx->sched_lat.active || ctx->sched_gran.active) {
		therm_lat = cpu_calc_thermal_latency(therm_scale, &param_sched);
		lat = cpu_calc_latency(pressure, demand, therm_lat, &param_cpu,
				       eff, &param_sched);
	}

	if (ctx->sched_lat.active)
		out_lat = math_quantize_u64(lat, param_sched.max_lat, quant);

	if (ctx->sched_gran.active) {
		gran = cpu_calc_granularity(lat, &param_cpu, &param_sched);
		out_gran = math_quantize_u64(gran, param_sched.max_gran, quant);
	}

	if (ctx->sched_wake.active) {
		wake = cpu_calc_wakeup(pressure, eff, &param_sched);
		out_wake = math_quantize_u64(wake, param_sched.max_wake, quant);
	}

	if (ctx->sched_mig.active) {
		mig = cpu_calc_migration(sample->some.velocity, pressure,
					 &param_sched);

		out_mig = math_quantize_u64(mig, param_sched.min_mig, quant);
	}

	if (ctx->sched_walt.active) {
		walt = cpu_calc_walt(pressure, &param_sched);

		out_walt = math_sanitize_u64(
			walt, (uint64_t)Q16_TO_INT(param_sched.min_walt));
	}

	if (ctx->sched_uclmax.active) {
		uclmax = cpu_calc_uclamp_max(therm_scale, ctx->level_bat,
					     &param_sched);

		out_uclmax = math_sanitize_u64(
			uclmax, (uint64_t)CPU_UCLAMP_MAX_CEILING);
	}

	if (ctx->sched_uclmin.active) {
		uclmin = cpu_calc_uclamp_min(pressure, therm_scale, &param_cpu,
					     eff, &param_sched);

		out_uclmin = math_sanitize_u64(
			uclmin, (uint64_t)Q16_TO_INT(param_sched.min_ucl));

		if (ctx->sched_uclmax.active && out_uclmin > out_uclmax)
			out_uclmin = out_uclmax;
	}

	node_update(&ctx->sched_lat, out_lat, false, &check_lat);
	node_update(&ctx->sched_gran, out_gran, false, &check_gran);
	node_update(&ctx->sched_wake, out_wake, false, &check_wake);
	node_update(&ctx->sched_mig, out_mig, false, &check_mig);
	node_update(&ctx->sched_walt, out_walt, false, &check_walt);
	node_update(&ctx->sched_uclmin, out_uclmin, false, &check_ucl);
	node_update(&ctx->sched_uclmax, out_uclmax, false, &check_ucl);
}

static inline void active_tick(struct context *ctx, const struct timespec *now)
{
	struct psi_sample sample;
	q16_t demand;
	q16_t pressure;
	q16_t therm_scale;

	if (update_display(ctx, now))
		return;

	if (!read_psi(ctx, now, &sample))
		return;

	calc_demand(ctx, now, &sample, &therm_scale, &demand, &pressure);
	if (should_apply_mapping(ctx, &sample, pressure))
		apply_mapping(ctx, &sample, therm_scale, demand, pressure);
}

static void dormant_tick(struct context *ctx)
{
#ifdef NDK_BUILD
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!update_display(ctx, &now))
		ctx->next_wake = POLL_MAX_MS;
#else
	ctx->next_wake = -1;
#endif // NDK_BUILD
}

void gov_run(struct context *ctx)
{
	struct timespec end;
	struct timespec now;
	q16_t dt;
	int ms;

	if (UNLIKELY(!ctx))
		return;

	if (UNLIKELY(ctx->dormant)) {
		dormant_tick(ctx);
		return;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	active_tick(ctx, &now);
	clock_gettime(CLOCK_MONOTONIC, &end);

	dt = time_dt_sec(&now, &end);
	ms = (int)((((q32_t)dt) * 1000) >> Q16_SHIFT);
	if (ctx->next_wake > ms)
		ctx->next_wake -= ms;
	else
		ctx->next_wake = 1;
}
