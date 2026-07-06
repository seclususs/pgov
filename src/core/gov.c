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

static inline void exec_cpu_logic(struct pg_context *RESTRICT ctx,
				  const struct timespec *RESTRICT now)
{
	struct pg_psi_data psi_data;
	int psi_status = pg_psi_read(&ctx->psi, &psi_data, now);

	if (UNLIKELY(psi_status != 0)) {
		LOGW("gov: psi read failed status=%d, init recov", psi_status);
		pg_epoll_rm_trg(ctx);

		if (ctx->trg_fd >= 0) {
			pg_psi_close_trg(ctx->trg_fd);
			ctx->trg_fd = -1;
		}

		if (pg_psi_recov(&ctx->psi, PG_PATH_PSI_CPU) == 0) {
			ctx->trg_fd = pg_psi_open_trg(PG_PATH_PSI_CPU,
						      PG_CFG_CTRL.thresh_us,
						      PG_CFG_CTRL.win_us);
			if (ctx->trg_fd >= 0)
				pg_epoll_add_trg(ctx);
		}

		return;
	}

	if (UNLIKELY(ctx->trg_fd < 0)) {
		ctx->trg_fd = pg_psi_open_trg(PG_PATH_PSI_CPU,
					      PG_CFG_CTRL.thresh_us,
					      PG_CFG_CTRL.win_us);
		if (ctx->trg_fd >= 0)
			pg_epoll_add_trg(ctx);
	}

	q16_t elapsed_bat_sec = pg_dt_sec(&ctx->last_bat, now);
	if (elapsed_bat_sec >= INT_TO_Q16(PG_CFG_CTRL.bat_chk_sec)) {
		pg_sensor_read_bat_cap(&ctx->bat_cap_sensor, &ctx->bat_lvl);
		pg_sensor_read_bat_temp(&ctx->bat_temp_sensor, &ctx->bat_temp);
		ctx->last_bat = *now;
	}

	q16_t cpu_temp;
	pg_sensor_read_cpu_temp(&ctx->cpu_temp_sensor, &cpu_temp);
	q16_t therm_scale = pg_thermal_update(&ctx->thermal_state, cpu_temp,
					      ctx->bat_temp, psi_data.some.cur,
					      &PG_CFG_THERMAL, now);

	q16_t trend_fact = pg_cpu_calc_trend_gain(psi_data.some.vel);
	q16_t dt_real = pg_dt_sec(&ctx->last_tick, now);
	q16_t dt_safe = pg_math_clamp(dt_real, 1, FLOAT_TO_Q16(0.1F));
	ctx->last_tick = *now;

	q16_t integ;
	q16_t integ_dt;
	pg_cpu_upd_integ_params(&ctx->load_state, ctx->bat_lvl, dt_safe,
				&PG_CFG_CPU, &integ, &integ_dt);

	bool struct_break = psi_data.some.nis > PG_CFG_CPU.nis_thresh;
	struct pg_demand_input demand_in = { .tgt_psi = psi_data.some.cur,
					     .vel = psi_data.some.vel,
					     .dt_real = dt_real,
					     .dt_safe = dt_safe,
					     .therm_scale = therm_scale,
					     .trend_fact = trend_fact,
					     .integ = integ,
					     .integ_dt = integ_dt,
					     .struct_break = struct_break };

	q16_t load_demand = pg_cpu_calc_load_demand(&ctx->load_state,
						    &demand_in, &PG_CFG_CPU);
	q16_t p_eff =
		pg_cpu_calc_eff_press(load_demand, trend_fact, &PG_CFG_CPU);

	int32_t next_poll = (int32_t)pg_poll_calc_next(&ctx->poll_state, p_eff,
						       psi_data.some.avg300,
						       psi_data.some.vel);

	if (pg_cpu_trans(&ctx->load_state, psi_data.some.cur, &PG_CFG_CPU)) {
		int32_t trans_poll = Q16_TO_INT(PG_CFG_CPU.trans_poll);
		if (next_poll > trans_poll)
			next_poll = trans_poll;
	}

	ctx->next_wake = next_poll;

	q16_t therm_lat = pg_cpu_calc_therm_lat(therm_scale, &LIM_CPU);
	q16_t lat;
	q16_t gran;
	pg_cpu_calc_lat_gran(p_eff, load_demand, therm_lat, &PG_CFG_CPU,
			     &LIM_CPU, &lat, &gran);

	q16_t wake = pg_cpu_calc_wakeup(p_eff, &PG_CFG_CPU, &LIM_CPU);
	q16_t mig = pg_cpu_calc_migration(psi_data.some.vel, p_eff, &LIM_CPU);
	q16_t walt = pg_cpu_calc_walt(p_eff, &LIM_CPU);
	q16_t ucl =
		pg_cpu_calc_uclamp(p_eff, therm_scale, &PG_CFG_CPU, &LIM_CPU);

	uint64_t lat_u64 =
		pg_math_san_quant_u64(lat, LIM_CPU.max_lat, PG_QUANT_NS);

	uint64_t gran_u64 =
		pg_math_san_quant_u64(gran, LIM_CPU.max_gran, PG_QUANT_NS);

	uint64_t wake_u64 =
		pg_math_san_quant_u64(wake, LIM_CPU.max_wake, PG_QUANT_NS);

	uint64_t mig_u64 =
		pg_math_san_quant_u64(mig, LIM_CPU.min_mig, PG_QUANT_NS);

	uint64_t walt_u64 =
		pg_math_san_u64(walt, (uint64_t)Q16_TO_INT(LIM_CPU.min_walt));

	uint64_t ucl_u64 =
		pg_math_san_u64(ucl, (uint64_t)Q16_TO_INT(LIM_CPU.min_uclamp));

	LOGD("gov: lat=%llu gran=%llu wake=%llu mig=%llu walt=%llu uclamp=%llu poll=%d",
	     (unsigned long long)lat_u64, (unsigned long long)gran_u64,
	     (unsigned long long)wake_u64, (unsigned long long)mig_u64,
	     (unsigned long long)walt_u64, (unsigned long long)ucl_u64,
	     next_poll);

	pg_sysfs_update(&ctx->sched_lat, lat_u64, false, &CHK_LAT);
	pg_sysfs_update(&ctx->sched_gran, gran_u64, false, &CHK_GRAN);
	pg_sysfs_update(&ctx->sched_wake, wake_u64, false, &CHK_WAKE);
	pg_sysfs_update(&ctx->sched_mig, mig_u64, false, &CHK_MIG);
	pg_sysfs_update(&ctx->sched_walt, walt_u64, false, &CHK_WALT);
	pg_sysfs_update(&ctx->sched_ucl, ucl_u64, false, &CHK_UCL);
}

void pg_gov_process_cpu(struct pg_context *ctx)
{
	struct timespec now;
	struct timespec end_calc;
	clock_gettime(CLOCK_MONOTONIC, &now);
	exec_cpu_logic(ctx, &now);
	clock_gettime(CLOCK_MONOTONIC, &end_calc);

	q16_t calc_s = pg_dt_sec(&now, &end_calc);
	int calc_ms = (int)((((q32_t)calc_s) * 1000) >> Q16_SHIFT);
	int adj_wake = ctx->next_wake - calc_ms;
	ctx->next_wake = (adj_wake > 0) ? adj_wake : 1;
}
