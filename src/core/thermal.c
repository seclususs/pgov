// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/thermal.h"
#include "pg/time.h"

void pg_thermal_init(struct pg_thermal_state *state)
{
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->integ = 0;
	state->prev_sat = 0;
	pg_kalman_init(&state->filter);
	state->filter.q_vel = FLOAT_TO_Q16(0.01F);
	state->filter.r_meas = FLOAT_TO_Q16(2.0F);
}

q16_t pg_thermal_update(struct pg_thermal_state *RESTRICT state, q16_t cpu_temp,
			q16_t bat_temp,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now)
{
	q16_t dt = pg_dt_sec(&state->last_tick, now);
	q16_t dt_s = pg_math_clamp(dt, FLOAT_TO_Q16(0.01F), Q16_ONE);
	state->last_tick = *now;

	q16_t t_tmp = pg_kalman_update(&state->filter, cpu_temp, dt_s);
	q16_t tmp_vel = state->filter.x_vel;
	q16_t d_tmp = bat_temp - cfg->temp_cool;
	q16_t r_tmp = cfg->temp_hot - cfg->temp_cool;
	q16_t sig = pg_math_clamp(q16_div(d_tmp, r_tmp), 0, Q16_ONE);

	q16_t k_p = cfg->kp_base + q16_mul(sig, cfg->kp_fast - cfg->kp_base);
	q16_t k_i = cfg->ki_base + q16_mul(sig, cfg->ki_fast - cfg->ki_base);
	q16_t k_d = cfg->kd_base + q16_mul(sig, cfg->kd_fast - cfg->kd_base);

	q16_t b_marg = cfg->limit_bat - bat_temp;
	if (b_marg < 0)
		b_marg = 0;

	q16_t c_marg = (b_marg < INT_TO_Q16(5)) ? (INT_TO_Q16(5) - b_marg) : 0;
	q16_t sp = cfg->limit_cpu - c_marg;

	q16_t err = t_tmp - sp;

	q16_t p_term = q16_mul(k_p, err);
	q16_t i_inc = q16_mul(q16_mul(k_i, err), dt_s);

	state->integ += i_inc;
	state->integ =
		pg_math_clamp(state->integ, INT_TO_Q16(-50), INT_TO_Q16(50));

	q16_t d_term = q16_mul(k_d, tmp_vel);
	q16_t u_raw = p_term + state->integ + d_term;
	q16_t u_sat = pg_math_clamp(u_raw, 0, INT_TO_Q16(100));
	if (ABS_Q16(u_raw - u_sat) > 65) {
		q16_t exc = u_raw - u_sat;
		state->integ -= q16_mul(q16_mul(exc, cfg->aw_k), dt_s);
	}

	state->prev_sat = u_sat;

	q16_t pid_sat = q16_div(u_sat, INT_TO_Q16(100));
	q16_t fscale = Q16_ONE - pid_sat;

	if (UNLIKELY(bat_temp >= cfg->limit_bat))
		return (fscale < FLOAT_TO_Q16(0.2F)) ? fscale :
						       FLOAT_TO_Q16(0.2F);

	return pg_math_clamp(fscale, FLOAT_TO_Q16(0.1F), Q16_ONE);
}
