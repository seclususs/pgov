// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/gov.h"
#include "pgov/param.h"
#include "pgov/log.h"
#include "pgov/epoll.h"
#include "pgov/compiler.h"
#include "pgov/cpu.h"
#include "pgov/math.h"
#include "pgov/poll.h"
#include "pgov/psi.h"
#include "pgov/sensor.h"
#include "pgov/node.h"
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
#define BAT_CHK_SEC 5
#define THERM_CHK_SEC 1
#define DISP_CHK_SEC 1
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

static const struct node_check check_ucl_max = {
	NODE_CHECK_ABSOLUTE,
	CHECK_UCL,
};

int pg_gov_init(struct pg_context *ctx)
{
	if (UNLIKELY(!ctx))
		return -EINVAL;

	clock_gettime(CLOCK_MONOTONIC, &ctx->last_tick);
	return 0;
}

static inline void relax_sched_tunables(struct pg_context *ctx)
{
	const struct cpu_limit *l = &param_sched;
	const uint64_t q_ns = QUANT_NS;

	if (ctx->sched_ucl.active && ctx->sched_ucl.last != 0)
		node_update(&ctx->sched_ucl, 0, true, NULL);

	if (ctx->sched_ucl_max.active) {
		uint64_t v_ucl = math_sanitize_u64(
			l->susp_ucl, (uint64_t)Q16_TO_INT(l->susp_ucl));
		if (ctx->sched_ucl_max.last != v_ucl)
			node_update(&ctx->sched_ucl_max, v_ucl, true, NULL);
	}

	if (ctx->sched_lat.active) {
		uint64_t v_lat =
			math_quantize_u64(l->max_lat, l->max_lat, q_ns);
		if (ctx->sched_lat.last != v_lat)
			node_update(&ctx->sched_lat, v_lat, true, NULL);
	}

	if (ctx->sched_gran.active) {
		uint64_t v_gran =
			math_quantize_u64(l->max_gran, l->max_gran, q_ns);
		if (ctx->sched_gran.last != v_gran)
			node_update(&ctx->sched_gran, v_gran, true, NULL);
	}

	if (ctx->sched_wake.active) {
		uint64_t v_wake =
			math_quantize_u64(l->max_wake, l->max_wake, q_ns);
		if (ctx->sched_wake.last != v_wake)
			node_update(&ctx->sched_wake, v_wake, true, NULL);
	}

	if (ctx->sched_mig.active) {
		uint64_t v_mig =
			math_quantize_u64(l->max_mig, l->min_mig, q_ns);
		if (ctx->sched_mig.last != v_mig)
			node_update(&ctx->sched_mig, v_mig, true, NULL);
	}

	if (ctx->sched_walt.active) {
		uint64_t v_wlt = math_sanitize_u64(
			l->min_walt, (uint64_t)Q16_TO_INT(l->min_walt));
		if (ctx->sched_walt.last != v_wlt)
			node_update(&ctx->sched_walt, v_wlt, true, NULL);
	}
}

static inline bool update_disp(struct pg_context *ctx,
			       const struct timespec *now)
{
	if (LIKELY(ctx->disp_state == DISP_ON) &&
	    pg_dt_sec(&ctx->last_disp, now) < INT_TO_Q16(DISP_CHK_SEC))
		return false;

	ctx->last_disp = *now;

	int32_t bl = 0;
	int ret = sensor_read_backlight(&ctx->bl_sensor, &bl);
	bool on = ((ret < 0) ? true : (bl > 0)) != 0;

	if (on && UNLIKELY(ctx->disp_state != DISP_ON)) {
		ctx->load_state.first_run = true;
		pg_kalman_reset(&ctx->psi.filter);
		ctx->psi.initial = true;
		ctx->disp_state = DISP_ON;
		ctx->remap_due = true;
	} else if (!on && (ctx->disp_state == DISP_ON ||
			   ctx->disp_state == DISP_UNKNOWN)) {
		ctx->disp_state = DISP_GRACE;
		ctx->last_dispoff = *now;
	} else if (!on && ctx->disp_state == DISP_GRACE) {
		if (pg_dt_sec(&ctx->last_dispoff, now) > INT_TO_Q16(10))
			ctx->disp_state = DISP_SUSPEND;
	}

	if (ctx->disp_state == DISP_SUSPEND) {
		relax_sched_tunables(ctx);

		ctx->next_wake = 5000;
		ctx->last_tick = *now;

#ifdef NDK_BUILD
		q16_t sweep_elaps = pg_dt_sec(&ctx->last_sweep, now);
		if (sweep_elaps > INT_TO_Q16(SWEEP_IVL_SEC)) {
			pg_sweep_run(ctx);
			clock_gettime(CLOCK_MONOTONIC, &ctx->last_sweep);
		}
#endif // NDK_BUILD

		return true;
	}

	return false;
}

static void arm_trigger(struct pg_context *ctx)
{
	int fd =
		psi_trigger_open(PSI_CPU_PATH, PSI_THRESHOLD_US, PSI_WINDOW_US);
	if (fd < 0)
		return;

	ctx->trg_fd = fd;
	if (pg_epoll_add_trg(ctx) < 0) {
		psi_trigger_close(fd);
		ctx->trg_fd = -1;
	}
}

static inline bool read_psi(struct pg_context *ctx, const struct timespec *now,
			    struct psi_sample *sample)
{
	int status = psi_read(&ctx->psi, sample, now);

	if (LIKELY(status == 0)) {
		if (UNLIKELY(ctx->psi_fails != 0)) {
			ctx->psi_fails = 0;
			LOGI("gov: psi read recovered");
		}
		return true;
	}

	if (ctx->psi_fails < PSI_FAIL_MAX) {
		ctx->psi_fails++;
		LOGW("gov: psi read failed status=%d, init recov", status);
	}

	uint32_t delay = 1000U << (ctx->psi_fails - 1);
	ctx->next_wake = (int)(delay > 30000U ? 30000U : delay);

	pg_epoll_rm_trg(ctx);

	if (ctx->trg_fd >= 0) {
		psi_trigger_close(ctx->trg_fd);
		ctx->trg_fd = -1;
	}

	psi_recover(&ctx->psi, PSI_CPU_PATH);

	arm_trigger(ctx);

	return false;
}

static inline void poll_battery_metrics(struct pg_context *ctx,
					const struct timespec *now)
{
	q16_t elaps_bat = pg_dt_sec(&ctx->last_bat, now);
	if (elaps_bat < INT_TO_Q16(BAT_CHK_SEC))
		return;

	sensor_read_battery_capacity(&ctx->bat_cap_sensor, &ctx->bat_lvl);
	sensor_read_battery_temp(&ctx->bat_temp_sensor, &ctx->bat_temp);
	ctx->last_bat = *now;

	if (UNLIKELY(ctx->trg_fd < 0))
		arm_trigger(ctx);
}

static inline void poll_thermal_metrics(struct pg_context *ctx,
					const struct timespec *now)
{
	q16_t elaps_therm = pg_dt_sec(&ctx->last_therm, now);
	if (elaps_therm < INT_TO_Q16(THERM_CHK_SEC))
		return;

	q16_t cpu_temp = 0;
	bool hold = false;

	if (LIKELY(ctx->cpu_temp_sensor.fd >= 0)) {
		if (sensor_read_cpu_temp(&ctx->cpu_temp_sensor, &cpu_temp) ==
		    0) {
			ctx->temp_fails = 0;
		} else if (ctx->temp_fails < TEMP_FAIL_MAX) {
			ctx->temp_fails++;
			hold = true;
		} else {
			cpu_temp = 0;
		}
	}

	if (LIKELY(!hold))
		ctx->cached_th_scl = thermal_update(&ctx->thermal_state,
						    cpu_temp, ctx->bat_temp,
						    &param_thermal, now);

	ctx->last_therm = *now;
	ctx->remap_due = true;
}

static inline void calc_demand(struct pg_context *ctx,
			       const struct timespec *now,
			       const struct psi_sample *sample,
			       q16_t *RESTRICT therm_scale,
			       q16_t *RESTRICT l_dem, q16_t *RESTRICT p_eff)
{
	poll_battery_metrics(ctx, now);
	poll_thermal_metrics(ctx, now);

	*therm_scale = ctx->cached_th_scl;

	struct cpu_efficiency *eff = &ctx->load_state.efficiency;
	cpu_update_efficiency(eff, ctx->bat_lvl, *therm_scale,
			      sample->some.avg300);

	q16_t dt_real = pg_dt_sec(&ctx->last_tick, now);
	q16_t dt_safe = math_clamp(dt_real, 1, FLOAT_TO_Q16(0.1F));
	ctx->last_tick = *now;

	q16_t i;
	q16_t i_dt;
	cpu_update_integral(&ctx->load_state, ctx->bat_lvl, dt_real, &i, &i_dt);

	q16_t t_fact = cpu_calc_trend_gain(sample->some.velocity);
	bool s_break = sample->some.nis > eff->nis_thresh;
	struct cpu_demand d_in = {
		.target_psi = sample->some.stall_rate,
		.velocity = sample->some.velocity,
		.dt_safe = dt_safe,
		.therm_scale = *therm_scale,
		.trend_factor = t_fact,
		.integral = i,
		.integral_dt = i_dt,
		.struct_break = s_break,
	};

	*l_dem = cpu_calc_load_demand(&ctx->load_state, &d_in, &param_cpu);
	*p_eff = cpu_calc_effective_pressure(*l_dem, t_fact, eff);
}

static inline bool gov_gate(struct pg_context *ctx,
			    const struct psi_sample *sample, q16_t p_eff)
{
	const struct cpu_param *c = &param_cpu;
	int32_t n_poll;
	int32_t t_poll;

	n_poll = (int32_t)pg_poll_calc_next(&ctx->poll_state, p_eff,
					    sample->some.avg300,
					    sample->some.velocity);
	ctx->next_wake = n_poll;

	bool moving = cpu_transient(&ctx->load_state, c);
	if (!moving && !ctx->remap_due)
		return false;

	if (moving) {
		t_poll = Q16_TO_INT(c->trans_poll);
		if (n_poll > t_poll)
			ctx->next_wake = t_poll;
	}

	ctx->load_state.psi_ref = ctx->load_state.psi_val;
	ctx->remap_due = false;
	return true;
}

static inline void apply_mapping(struct pg_context *ctx,
				 const struct psi_sample *sample,
				 q16_t therm_scale, q16_t l_dem, q16_t p_eff)
{
	const struct cpu_param *c = &param_cpu;
	const struct cpu_limit *l = &param_sched;
	const uint64_t q_ns = QUANT_NS;
	struct cpu_efficiency *eff = &ctx->load_state.efficiency;

	uint64_t v_lat = 0;
	uint64_t v_gran = 0;
	uint64_t v_wake = 0;
	uint64_t v_mig = 0;
	uint64_t v_wlt = 0;
	uint64_t v_ucl = 0;
	uint64_t v_ucl_max = CPU_UCLAMP_MAX_CEILING;

	q16_t gran = 0;
	q16_t lat = 0;
	q16_t mig = 0;
	q16_t therm_lat = 0;
	q16_t ucl = 0;
	q16_t ucl_max = 0;
	q16_t wake = 0;
	q16_t wlt = 0;

	if (ctx->sched_lat.active || ctx->sched_gran.active) {
		therm_lat = cpu_calc_thermal_latency(therm_scale, l);
		lat = cpu_calc_latency(p_eff, l_dem, therm_lat, c, eff, l);

		if (ctx->sched_lat.active)
			v_lat = math_quantize_u64(lat, l->max_lat, q_ns);
	}

	if (ctx->sched_gran.active) {
		gran = cpu_calc_granularity(lat, c, l);
		v_gran = math_quantize_u64(gran, l->max_gran, q_ns);
	}

	if (ctx->sched_wake.active) {
		wake = cpu_calc_wakeup(p_eff, eff, l);
		v_wake = math_quantize_u64(wake, l->max_wake, q_ns);
	}

	if (ctx->sched_mig.active) {
		mig = cpu_calc_migration(sample->some.velocity, p_eff, l);
		v_mig = math_quantize_u64(mig, l->min_mig, q_ns);
	}

	if (ctx->sched_walt.active) {
		wlt = cpu_calc_walt(p_eff, l);
		v_wlt = math_sanitize_u64(wlt,
					  (uint64_t)Q16_TO_INT(l->min_walt));
	}

	if (ctx->sched_ucl_max.active) {
		ucl_max = cpu_calc_uclamp_max(therm_scale, ctx->bat_lvl, l);
		v_ucl_max = math_sanitize_u64(ucl_max,
					      (uint64_t)CPU_UCLAMP_MAX_CEILING);
	}

	if (ctx->sched_ucl.active) {
		ucl = cpu_calc_uclamp_min(p_eff, therm_scale, c, eff, l);
		v_ucl = math_sanitize_u64(ucl,
					  (uint64_t)Q16_TO_INT(l->min_ucl));
		if (ctx->sched_ucl_max.active && v_ucl > v_ucl_max)
			v_ucl = v_ucl_max;
	}

	LOGD("gov: lat=%llu gran=%llu wake=%llu mig=%llu walt=%llu uclamp=%llu uclamp_max=%llu poll=%d",
	     (unsigned long long)v_lat, (unsigned long long)v_gran,
	     (unsigned long long)v_wake, (unsigned long long)v_mig,
	     (unsigned long long)v_wlt, (unsigned long long)v_ucl,
	     (unsigned long long)v_ucl_max, ctx->next_wake);

	node_update(&ctx->sched_lat, v_lat, false, &check_lat);
	node_update(&ctx->sched_gran, v_gran, false, &check_gran);
	node_update(&ctx->sched_wake, v_wake, false, &check_wake);
	node_update(&ctx->sched_mig, v_mig, false, &check_mig);
	node_update(&ctx->sched_walt, v_wlt, false, &check_walt);
	node_update(&ctx->sched_ucl, v_ucl, false, &check_ucl);
	node_update(&ctx->sched_ucl_max, v_ucl_max, false, &check_ucl_max);
}

static inline void exec_gov_logic(struct pg_context *ctx,
				  const struct timespec *now)
{
	if (update_disp(ctx, now))
		return;

	struct psi_sample sample;
	if (!read_psi(ctx, now, &sample))
		return;

	q16_t l_dem;
	q16_t p_eff;
	q16_t therm_scale;
	calc_demand(ctx, now, &sample, &therm_scale, &l_dem, &p_eff);
	if (gov_gate(ctx, &sample, p_eff))
		apply_mapping(ctx, &sample, therm_scale, l_dem, p_eff);
}

static void dormant_tick(struct pg_context *ctx)
{
#ifdef NDK_BUILD
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!update_disp(ctx, &now))
		ctx->next_wake = MAX_POLL_MS;
#else
	ctx->next_wake = -1;
#endif // NDK_BUILD
}

void pg_gov_process(struct pg_context *ctx)
{
	if (UNLIKELY(!ctx))
		return;

	if (UNLIKELY(ctx->dormant)) {
		dormant_tick(ctx);
		return;
	}

	struct timespec now;
	struct timespec end_calc;
	clock_gettime(CLOCK_MONOTONIC, &now);
	exec_gov_logic(ctx, &now);
	clock_gettime(CLOCK_MONOTONIC, &end_calc);

	q16_t calc_s = pg_dt_sec(&now, &end_calc);
	int calc_ms = (int)((((q32_t)calc_s) * 1000) >> Q16_SHIFT);
	int adj_wake = ctx->next_wake - calc_ms;
	ctx->next_wake = (adj_wake > 0) ? adj_wake : 1;
}
