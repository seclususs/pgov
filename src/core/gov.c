// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/gov.h"
#include "pg/config.h"
#include "pg/log.h"
#include "epoll.h"
#include "paths.h"
#include "compiler.h"
#include "pg/cpu.h"
#include "pg/math.h"
#include "pg/poll.h"
#include "psi.h"
#include "sensor.h"
#include "sysfs.h"
#include "pg/sweep.h"
#include "pg/thermal.h"
#include "pg/time.h"
#include <unistd.h>

static const struct pg_sysfs_chk CHK_LAT = { PG_CHK_REL, PG_CHK_LAT };
static const struct pg_sysfs_chk CHK_GRAN = { PG_CHK_REL, PG_CHK_GRAN };
static const struct pg_sysfs_chk CHK_WAKE = { PG_CHK_REL, PG_CHK_WAKE };
static const struct pg_sysfs_chk CHK_MIG = { PG_CHK_ABS, PG_CHK_MIG };
static const struct pg_sysfs_chk CHK_WALT = { PG_CHK_ABS, PG_CHK_WALT };
static const struct pg_sysfs_chk CHK_UCL = { PG_CHK_ABS, PG_CHK_UCL };

int pg_gov_init(struct pg_context *ctx)
{
	clock_gettime(CLOCK_MONOTONIC, &ctx->last_tick);
	return 0;
}

static inline bool update_disp(struct pg_context *RESTRICT ctx,
			       const struct timespec *RESTRICT now)
{
	int32_t bl = 0;
	int ret = pg_sensor_read_bl(&ctx->bl_sensor, &bl);
	bool on = ((ret < 0) ? true : (bl > 0)) != 0;

	if (on && UNLIKELY(ctx->disp_state != PG_DISP_ON)) {
		ctx->load_state.first_run = true;
		pg_kalman_reset(&ctx->psi.filter);
		ctx->psi.first_run = true;
		ctx->disp_state = PG_DISP_ON;
	} else if (!on && (ctx->disp_state == PG_DISP_ON ||
			   ctx->disp_state == PG_DISP_UNKNOWN)) {
		ctx->disp_state = PG_DISP_GRACE;
		ctx->last_dispoff = *now;
	} else if (!on && ctx->disp_state == PG_DISP_GRACE) {
		if (pg_dt_sec(&ctx->last_dispoff, now) > INT_TO_Q16(10))
			ctx->disp_state = PG_DISP_SUSPEND;
	}

	if (ctx->disp_state == PG_DISP_SUSPEND) {
		ctx->next_wake = 5000;
		ctx->last_tick = *now;

#if defined(NDK_BUILD)
		q16_t sweep_elaps = pg_dt_sec(&ctx->last_sweep, now);
		if (sweep_elaps > INT_TO_Q16(PG_SWEEP_IVL_SEC)) {
			pg_sweep_run(ctx);
			clock_gettime(CLOCK_MONOTONIC, &ctx->last_sweep);
		}
#endif // NDK_BUILD

		return true;
	}

	return false;
}

static inline bool read_psi(struct pg_context *RESTRICT ctx,
			    const struct timespec *RESTRICT now,
			    struct pg_psi_data *RESTRICT psi)
{
	int status = pg_psi_read(&ctx->psi, psi, now);
	if (LIKELY(status == 0))
		return true;

	LOGW("gov: psi read failed status=%d, init recov", status);
	pg_epoll_rm_trg(ctx);

	if (ctx->trg_fd >= 0) {
		pg_psi_close_trg(ctx->trg_fd);
		ctx->trg_fd = -1;
	}

	if (pg_psi_recov(&ctx->psi, PG_PATH_PSI_CPU) == 0) {
		ctx->trg_fd = pg_psi_open_trg(
			PG_PATH_PSI_CPU, PG_PSI_THRESHOLD_US, PG_PSI_WINDOW_US);

		if (ctx->trg_fd >= 0)
			pg_epoll_add_trg(ctx);

		return false;
	}

	if (UNLIKELY(ctx->trg_fd < 0)) {
		ctx->trg_fd = pg_psi_open_trg(
			PG_PATH_PSI_CPU, PG_PSI_THRESHOLD_US, PG_PSI_WINDOW_US);

		if (ctx->trg_fd >= 0)
			pg_epoll_add_trg(ctx);
	}

	return false;
}

static inline void calc_demand(struct pg_context *RESTRICT ctx,
			       const struct timespec *RESTRICT now,
			       const struct pg_psi_data *RESTRICT psi,
			       q16_t *RESTRICT th_scl, q16_t *RESTRICT l_dem,
			       q16_t *RESTRICT p_eff)
{
	q16_t elaps_bat = pg_dt_sec(&ctx->last_bat, now);
	if (elaps_bat >= INT_TO_Q16(PG_BAT_CHK_SEC)) {
		pg_sensor_read_bat_cap(&ctx->bat_cap_sensor, &ctx->bat_lvl);
		pg_sensor_read_bat_temp(&ctx->bat_temp_sensor, &ctx->bat_temp);
		ctx->last_bat = *now;
	}

	q16_t cpu_temp;
	pg_sensor_read_cpu_temp(&ctx->cpu_temp_sensor, &cpu_temp);
	*th_scl = pg_thermal_update(&ctx->thermal_state, cpu_temp,
				    ctx->bat_temp, &CFG_THERMAL, now);

	struct pg_cpu_eff *eff = &ctx->load_state.eff;
	pg_cpu_upd_eff(eff, ctx->bat_lvl, *th_scl, psi->some.avg300);

	q16_t dt_real = pg_dt_sec(&ctx->last_tick, now);
	q16_t dt_safe = pg_math_clamp(dt_real, 1, FLOAT_TO_Q16(0.1F));
	ctx->last_tick = *now;

	q16_t i;
	q16_t i_dt;
	pg_cpu_upd_intg(&ctx->load_state, ctx->bat_lvl, dt_real, &i, &i_dt);

	q16_t t_fact = pg_cpu_calc_trend_gain(psi->some.vel);
	bool s_break = psi->some.nis > CFG_CPU.nis_thresh;
	struct pg_demand_input d_in = { .tgt_psi = psi->some.cur,
					.vel = psi->some.vel,
					.dt_real = dt_real,
					.dt_safe = dt_safe,
					.therm_scale = *th_scl,
					.trend_fact = t_fact,
					.integ = i,
					.integ_dt = i_dt,
					.struct_break = s_break };

	*l_dem = pg_cpu_calc_load_demand(&ctx->load_state, &d_in, &CFG_CPU);
	*p_eff = pg_cpu_calc_eff_press(*l_dem, t_fact, eff);
}

static inline void update_sysfs(struct pg_context *RESTRICT ctx,
				const struct pg_psi_data *RESTRICT psi,
				q16_t th_scl, q16_t l_dem, q16_t p_eff)
{
	const struct pg_cpu_cfg *c = &CFG_CPU;
	const struct pg_cpu_lim *l = &LIM_CPU;
	const uint64_t q_ns = PG_QUANT_NS;
	struct pg_cpu_eff *eff = &ctx->load_state.eff;

	uint64_t v_lat = 0;
	uint64_t v_gran = 0;
	uint64_t v_wake = 0;
	uint64_t v_mig = 0;
	uint64_t v_wlt = 0;
	uint64_t v_ucl = 0;

	q16_t th = 0;
	q16_t lat = 0;
	q16_t gran = 0;
	q16_t wake = 0;
	q16_t mig = 0;
	q16_t wlt = 0;
	q16_t ucl = 0;

	int32_t n_poll;
	int32_t t_poll;

	n_poll = (int32_t)pg_poll_calc_next(&ctx->poll_state, p_eff,
					    psi->some.avg300, psi->some.vel);

	ctx->next_wake = n_poll;

	if (!pg_cpu_trans(&ctx->load_state, psi->some.cur, c))
		return;

	t_poll = Q16_TO_INT(c->trans_poll);
	if (n_poll > t_poll)
		ctx->next_wake = t_poll;

	if (ctx->sched_lat.active || ctx->sched_gran.active) {
		th = pg_cpu_calc_therm_lat(th_scl, l);
		lat = pg_cpu_calc_lat(p_eff, l_dem, th, c, eff, l);

		if (ctx->sched_lat.active)
			v_lat = pg_math_san_quant_u64(lat, l->max_lat, q_ns);
	}

	if (ctx->sched_gran.active) {
		gran = pg_cpu_calc_gran(lat, c, l);
		v_gran = pg_math_san_quant_u64(gran, l->max_gran, q_ns);
	}

	if (ctx->sched_wake.active) {
		wake = pg_cpu_calc_wakeup(p_eff, eff, l);
		v_wake = pg_math_san_quant_u64(wake, l->max_wake, q_ns);
	}

	if (ctx->sched_mig.active) {
		mig = pg_cpu_calc_migration(psi->some.vel, p_eff, l);
		v_mig = pg_math_san_quant_u64(mig, l->min_mig, q_ns);
	}

	if (ctx->sched_walt.active) {
		wlt = pg_cpu_calc_walt(p_eff, l);
		v_wlt = pg_math_san_u64(wlt, (uint64_t)Q16_TO_INT(l->min_walt));
	}

	if (ctx->sched_ucl.active) {
		ucl = pg_cpu_calc_uclamp(p_eff, th_scl, c, eff, l);
		v_ucl = pg_math_san_u64(ucl, (uint64_t)Q16_TO_INT(l->min_ucl));
	}

	LOGD("gov: lat=%llu gran=%llu wake=%llu mig=%llu walt=%llu uclamp=%llu poll=%d",
	     (unsigned long long)v_lat, (unsigned long long)v_gran,
	     (unsigned long long)v_wake, (unsigned long long)v_mig,
	     (unsigned long long)v_wlt, (unsigned long long)v_ucl,
	     ctx->next_wake);

	pg_sysfs_update(&ctx->sched_lat, v_lat, false, &CHK_LAT);
	pg_sysfs_update(&ctx->sched_gran, v_gran, false, &CHK_GRAN);
	pg_sysfs_update(&ctx->sched_wake, v_wake, false, &CHK_WAKE);
	pg_sysfs_update(&ctx->sched_mig, v_mig, false, &CHK_MIG);
	pg_sysfs_update(&ctx->sched_walt, v_wlt, false, &CHK_WALT);
	pg_sysfs_update(&ctx->sched_ucl, v_ucl, false, &CHK_UCL);
}

static inline void exec_gov_logic(struct pg_context *RESTRICT ctx,
				  const struct timespec *RESTRICT now)
{
	if (update_disp(ctx, now))
		return;

	struct pg_psi_data psi;
	if (!read_psi(ctx, now, &psi))
		return;

	q16_t th_scl;
	q16_t l_dem;
	q16_t p_eff;
	calc_demand(ctx, now, &psi, &th_scl, &l_dem, &p_eff);
	update_sysfs(ctx, &psi, th_scl, l_dem, p_eff);
}

void pg_gov_process(struct pg_context *ctx)
{
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
