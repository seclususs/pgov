// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/kalman.h"

#define STALE_SEC 5

void kalman_init(struct kalman_state *state)
{
	if (UNLIKELY(!state))
		return;

	kalman_reset(state);
}

void kalman_reset(struct kalman_state *state)
{
	if (UNLIKELY(!state))
		return;

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

q16_t kalman_update(struct kalman_state *state, q16_t meas, q16_t dt_sec)
{
	q16_t boost;
	q16_t calc_r;
	q16_t cy_q16;
	q16_t dt;
	q16_t excess;
	q16_t inv_s;
	q16_t k0;
	q16_t k1;
	q16_t next_q;
	q16_t p00_q16;
	q16_t q_pos;
	q16_t s;
	q16_t s_pred;
	q16_t term_k0;
	q16_t x_pos_pred;
	q16_t x_vel_pred;
	q16_t y;
	q16_t y_clamp;
	q16_t z;
	q32_t a_ema;
	q32_t dt2;
	q32_t dt3;
	q32_t dt4;
	q32_t dt_q32;
	q32_t f_p00;
	q32_t f_p01;
	q32_t f_p10;
	q32_t f_p11;
	q32_t inv_a;
	q32_t nis_q32;
	q32_t nis_shift;
	q32_t p00_pred;
	q32_t p01_pred;
	q32_t p10_pred;
	q32_t p11_pred;
	q32_t q00;
	q32_t q01;
	q32_t q10;
	q32_t q11;
	q32_t q_pos_dt;
	q32_t q_pos_q32;
	q32_t q_vel_q32;
	q32_t r_y_q32;
	q32_t r_y_sq;
	q32_t y_q32;
	q32_t y_sq;

	if (UNLIKELY(!state))
		return 0;

	z = math_clamp(meas, 0, INT_TO_Q16(500));

	if (UNLIKELY(dt_sec > INT_TO_Q16(STALE_SEC)))
		kalman_reset(state);

	if (UNLIKELY(state->first_run)) {
		state->x_pos = z;
		state->x_vel = 0;
		state->first_run = false;
		return z;
	}

	dt = (dt_sec > FLOAT_TO_Q16(0.0001F)) ? dt_sec : FLOAT_TO_Q16(0.0001F);

	dt_q32 = Q16_TO_Q32(dt);
	dt2 = math_mul_q32(dt_q32, dt_q32);
	dt3 = math_mul_q32(dt2, dt_q32);
	dt4 = math_mul_q32(dt3, dt_q32);

	x_pos_pred = state->x_pos + math_mul_q16(state->x_vel, dt);
	x_vel_pred = state->x_vel;

	q_pos = math_mul_q16(state->q_vel, FLOAT_TO_Q16(0.01F));
	q_pos_q32 = Q16_TO_Q32(q_pos);
	q_vel_q32 = Q16_TO_Q32(state->q_vel);

	q_pos_dt = math_mul_q32(q_pos_q32, dt_q32);
	q00 = (math_mul_q32(q_vel_q32, dt4) / 4) + q_pos_dt;
	q01 = math_mul_q32(q_vel_q32, dt3) / 2;
	q10 = q01;
	q11 = math_mul_q32(q_vel_q32, dt2) + q_pos_dt;

	f_p00 = state->p00 + math_mul_q32(state->p10, dt_q32);
	f_p01 = state->p01 + math_mul_q32(state->p11, dt_q32);
	f_p10 = state->p10;
	f_p11 = state->p11;

	p00_pred = f_p00 + math_mul_q32(f_p01, dt_q32) + q00;
	p01_pred = f_p01 + q01;
	p10_pred = f_p10 + math_mul_q32(f_p11, dt_q32) + q10;
	p11_pred = f_p11 + q11;

	y = z - x_pos_pred;
	y_clamp = math_clamp(y, INT_TO_Q16(-20), INT_TO_Q16(20));
	y_q32 = Q16_TO_Q32(y_clamp);
	y_sq = math_mul_q32(y_q32, y_q32);

	a_ema = Q16_TO_Q32(FLOAT_TO_Q16(0.05F));
	inv_a = Q16_TO_Q32(FLOAT_TO_Q16(0.95F));
	state->cov_y =
		math_mul_q32(inv_a, state->cov_y) + math_mul_q32(a_ema, y_sq);

	cy_q16 = Q32_TO_Q16(state->cov_y);
	p00_q16 = Q32_TO_Q16(p00_pred);

	calc_r = cy_q16 - p00_q16;
	state->r_meas = math_clamp(calc_r, FLOAT_TO_Q16(0.1F), INT_TO_Q16(50));

	s_pred = p00_q16 + state->r_meas;
	next_q = math_mul_q16(FLOAT_TO_Q16(0.95F), state->q_vel);

	if (cy_q16 > s_pred) {
		excess = cy_q16 - s_pred;
		boost = math_mul_q16(FLOAT_TO_Q16(0.05F), excess);
		next_q = state->q_vel + boost;
	}

	state->q_vel = math_clamp(next_q, FLOAT_TO_Q16(0.01F), INT_TO_Q16(20));

	s = s_pred;
	inv_s = (s > 0) ? math_div_q16(Q16_ONE, s) : 0;

	k0 = math_mul_q16(p00_q16, inv_s);
	k1 = math_mul_q16(Q32_TO_Q16(p10_pred), inv_s);

	state->x_pos = x_pos_pred + math_mul_q16(k0, y);
	state->x_vel = x_vel_pred + math_mul_q16(k1, y);

	term_k0 = Q16_ONE - k0;
	state->p00 = math_mul_q32(Q16_TO_Q32(term_k0), p00_pred);
	state->p01 = math_mul_q32(Q16_TO_Q32(term_k0), p01_pred);

	state->p10 = p10_pred - math_mul_q32(Q16_TO_Q32(k1), p00_pred);
	state->p11 = p11_pred - math_mul_q32(Q16_TO_Q32(k1), p01_pred);

	r_y_q32 = Q16_TO_Q32(y);
	r_y_sq = math_mul_q32(r_y_q32, r_y_q32);
	nis_q32 = math_mul_q32(r_y_sq, Q16_TO_Q32(inv_s));
	nis_shift = nis_q32 >> Q16_SHIFT;
	state->nis = (nis_shift > INT32_MAX) ? INT32_MAX : (q16_t)nis_shift;

	return (state->x_pos > 0) ? state->x_pos : 0;
}
