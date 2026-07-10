// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/kalman.h"

void pg_kalman_init(struct pg_kalman_state *RESTRICT state)
{
	pg_kalman_reset(state);
}

void pg_kalman_reset(struct pg_kalman_state *state)
{
	state->first_run = true;
	state->x_pos = 0;
	state->x_vel = 0;

	state->p00 = Q16_TO_Q32(INT_TO_Q16(100));
	state->p01 = 0;
	state->p10 = 0;
	state->p11 = Q16_TO_Q32(INT_TO_Q16(100));

	state->q_vel = FLOAT_TO_Q16(1.0F);
	state->r_meas = FLOAT_TO_Q16(4.0F);
	state->cov_y = Q16_TO_Q32(FLOAT_TO_Q16(16.0F));

	state->nis = 0;
}

q16_t pg_kalman_update(struct pg_kalman_state *state, q16_t z_meas,
		       q16_t dt_sec)
{
	q16_t z = pg_math_clamp(z_meas, 0, INT_TO_Q16(500));

	if (UNLIKELY(dt_sec > INT_TO_Q16(5)))
		pg_kalman_reset(state);

	if (UNLIKELY(state->first_run)) {
		state->x_pos = z;
		state->x_vel = 0;
		state->first_run = false;
		return z;
	}

	q16_t dt = (dt_sec > FLOAT_TO_Q16(0.0001F)) ? dt_sec :
						      FLOAT_TO_Q16(0.0001F);

	q32_t dt_q32 = Q16_TO_Q32(dt);
	q32_t dt2 = q32_mul(dt_q32, dt_q32);
	q32_t dt3 = q32_mul(dt2, dt_q32);
	q32_t dt4 = q32_mul(dt3, dt_q32);

	q16_t x_pos_pred = state->x_pos + q16_mul(state->x_vel, dt);
	q16_t x_vel_pred = state->x_vel;

	q16_t q_pos = q16_mul(state->q_vel, FLOAT_TO_Q16(0.01F));
	q32_t q_pos_q32 = Q16_TO_Q32(q_pos);
	q32_t q_vel_q32 = Q16_TO_Q32(state->q_vel);

	q32_t q00 = (q32_mul(q_vel_q32, dt4) / 4) + q32_mul(q_pos_q32, dt_q32);
	q32_t q01 = q32_mul(q_vel_q32, dt3) / 2;
	q32_t q10 = q01;
	q32_t q11 = q32_mul(q_vel_q32, dt2) + q32_mul(q_pos_q32, dt_q32);

	q32_t f_p00 = state->p00 + q32_mul(state->p10, dt_q32);
	q32_t f_p01 = state->p01 + q32_mul(state->p11, dt_q32);
	q32_t f_p10 = state->p10;
	q32_t f_p11 = state->p11;

	q32_t p00_pred = f_p00 + q32_mul(f_p01, dt_q32) + q00;
	q32_t p01_pred = f_p01 + q01;
	q32_t p10_pred = f_p10 + q32_mul(f_p11, dt_q32) + q10;
	q32_t p11_pred = f_p11 + q11;

	q16_t y = z - x_pos_pred;
	q16_t y_clamp = pg_math_clamp(y, INT_TO_Q16(-20), INT_TO_Q16(20));
	q32_t y_q32 = Q16_TO_Q32(y_clamp);
	q32_t y_sq = q32_mul(y_q32, y_q32);

	q32_t a_ema = Q16_TO_Q32(FLOAT_TO_Q16(0.05F));
	q32_t inv_a = Q16_TO_Q32(FLOAT_TO_Q16(0.95F));
	state->cov_y = q32_mul(inv_a, state->cov_y) + q32_mul(a_ema, y_sq);

	q16_t cy_q16 = Q32_TO_Q16(state->cov_y);
	q16_t p00_q16 = Q32_TO_Q16(p00_pred);

	q16_t calc_r = cy_q16 - p00_q16;
	state->r_meas =
		pg_math_clamp(calc_r, FLOAT_TO_Q16(0.1F), INT_TO_Q16(50));

	q16_t s_pred = p00_q16 + state->r_meas;
	q16_t next_q = q16_mul(FLOAT_TO_Q16(0.95F), state->q_vel);
	if (cy_q16 > s_pred)
		next_q = state->q_vel +
			 q16_mul(FLOAT_TO_Q16(0.05F), cy_q16 - s_pred);

	state->q_vel =
		pg_math_clamp(next_q, FLOAT_TO_Q16(0.01F), INT_TO_Q16(20));

	q16_t s = p00_q16 + state->r_meas;
	q16_t inv_s = (s > 0) ? q16_div(Q16_ONE, s) : 0;

	q16_t k0 = q16_mul(p00_q16, inv_s);
	q16_t k1 = q16_mul(Q32_TO_Q16(p10_pred), inv_s);

	state->x_pos = x_pos_pred + q16_mul(k0, y);
	state->x_vel = x_vel_pred + q16_mul(k1, y);

	q16_t term_k0 = Q16_ONE - k0;
	state->p00 = q32_mul(Q16_TO_Q32(term_k0), p00_pred);
	state->p01 = q32_mul(Q16_TO_Q32(term_k0), p01_pred);

	state->p10 = p10_pred - q32_mul(Q16_TO_Q32(k1), p00_pred);
	state->p11 = p11_pred - q32_mul(Q16_TO_Q32(k1), p01_pred);

	q32_t r_y_q32 = Q16_TO_Q32(y);
	q32_t r_y_sq = q32_mul(r_y_q32, r_y_q32);
	state->nis = Q32_TO_Q16(q32_mul(r_y_sq, Q16_TO_Q32(inv_s)));

	return (state->x_pos > 0) ? state->x_pos : 0;
}
