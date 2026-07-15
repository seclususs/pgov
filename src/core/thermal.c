// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/thermal.h"
#include "pg/time.h"

void pg_thermal_init(struct pg_thermal_state *state)
{
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->integ = 0;
	state->prev_sat = 0;
	state->sat_count = 0;
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

	q16_t tmp = pg_kalman_update(&state->filter, cpu_temp, dt_s);
	q16_t vel = state->filter.x_vel;

	q16_t b_marg = cfg->limit_bat - bat_temp;
	if (b_marg < 0)
		b_marg = 0;

	q16_t c_marg = (b_marg < INT_TO_Q16(5)) ? (INT_TO_Q16(5) - b_marg) : 0;
	q16_t sp = cfg->limit_cpu - c_marg;
	q16_t err = tmp - sp;

	q16_t t_dist = sp - tmp;
	if (t_dist < FLOAT_TO_Q16(0.1F))
		t_dist = FLOAT_TO_Q16(0.1F);

	q16_t k_p = cfg->kp_base + q16_div(cfg->kp_base, t_dist);
	q16_t k_i = cfg->ki_base + q16_div(cfg->ki_base, t_dist);
	q16_t v_bst = (vel > 0) ? q16_mul(FLOAT_TO_Q16(2.0F), vel) : 0;
	q16_t k_d = cfg->kd_base + v_bst;

	state->integ += q16_mul(q16_mul(k_i, err), dt_s);
	state->integ =
		pg_math_clamp(state->integ, INT_TO_Q16(-50), INT_TO_Q16(50));

	q16_t u_raw = q16_mul(k_p, err) + state->integ + q16_mul(k_d, vel);
	q16_t u_sat = pg_math_clamp(u_raw, 0, INT_TO_Q16(100));

	if (state->prev_sat >= INT_TO_Q16(100)) {
		if (state->sat_count < 50)
			state->sat_count++;
	} else {
		state->sat_count = 0;
	}

	q16_t aw_k = FLOAT_TO_Q16(1.95F);
	if (state->sat_count > 0) {
		q16_t aw_bst = q16_mul(FLOAT_TO_Q16(0.1F),
				       INT_TO_Q16(state->sat_count));
		aw_k += aw_bst;
	}

	if (ABS_Q16(u_raw - u_sat) > 65) {
		q16_t exc = u_raw - u_sat;
		q16_t drain = q16_mul(q16_mul(exc, aw_k), dt_s);

		if ((state->integ > 0 && drain > state->integ) ||
		    (state->integ < 0 && drain < state->integ))
			state->integ = 0;
		else
			state->integ -= drain;
	}

	state->prev_sat = u_sat;
	q16_t fscale = Q16_ONE - q16_div(u_sat, INT_TO_Q16(100));

	if (UNLIKELY(bat_temp >= cfg->limit_bat))
		return (fscale < FLOAT_TO_Q16(0.2F)) ? fscale :
						       FLOAT_TO_Q16(0.2F);

	return pg_math_clamp(fscale, FLOAT_TO_Q16(0.1F), Q16_ONE);
}
