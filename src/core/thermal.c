// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/thermal.h"
#include "pg/time.h"

void pg_thermal_init(struct pg_thermal_state *state)
{
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->integ = 0;
	state->prev_pv = 0;
	state->prev_d = 0;
	state->prev_sat = 0;

	state->ff.prev_y = 0;
	state->ff.prev_u = 0;
	state->ff.first_run = true;

	state->smith.ndelay = 0;
	state->smith.head = 0;
	state->smith.tail = 0;
	state->smith.count = 0;

	for (size_t i = 0; i < SMITH_BUFFER; ++i) {
		state->smith.buf[i].value = 0;
		state->smith.buf[i].ts = state->last_tick;
	}
}

static inline q16_t pg_thermal_lead_lag(struct pg_lead_lag_state *state,
					q16_t input, q16_t dt, q16_t gain,
					q16_t t_lead, q16_t t_lag)
{
	if (UNLIKELY(state->first_run)) {
		state->prev_u = input;
		state->prev_y = q16_mul(input, gain);
		state->first_run = false;
		return state->prev_y;
	}

	q16_t t2_lag = q16_mul(INT_TO_Q16(2), t_lag);
	q16_t t2_lead = q16_mul(INT_TO_Q16(2), t_lead);

	q16_t denom = t2_lag + dt;
	q16_t c_py = t2_lag - dt;
	q16_t c_in = q16_mul(gain, t2_lead + dt);
	q16_t c_pu = q16_mul(gain, t2_lead - dt);

	q16_t n1 = q16_mul(c_in, input);
	q16_t n2 = q16_mul(c_pu, state->prev_u);
	q16_t n3 = q16_mul(c_py, state->prev_y);

	q16_t num = n1 - n2 + n3;
	q16_t output = q16_div(num, denom);

	state->prev_u = input;
	state->prev_y = output;

	return output;
}

static inline void pg_thermal_smith(struct pg_smith_pred *RESTRICT state,
				    q16_t u_control, q16_t dt, q16_t k_gain,
				    q16_t tau, q16_t delay_sec,
				    const struct timespec *RESTRICT now,
				    q16_t *RESTRICT ndelay,
				    q16_t *RESTRICT delay)
{
	q16_t alpha = q16_div(dt, tau + dt);
	q16_t term1 = q16_mul(alpha, q16_mul(u_control, k_gain));
	q16_t term2 = q16_mul(Q16_ONE - alpha, state->ndelay);
	q16_t y_nd = term1 + term2;

	state->ndelay = y_nd;
	if (state->count == SMITH_BUFFER)
		state->tail = (state->tail + 1) & (SMITH_BUFFER - 1);
	else
		state->count += 1;

	state->buf[state->head].value = y_nd;
	state->buf[state->head].ts = *now;
	state->head = (state->head + 1) & (SMITH_BUFFER - 1);

	while (true) {
		size_t n_tail = (state->tail + 1) & (SMITH_BUFFER - 1);
		if (n_tail == state->head)
			break;

		const struct pg_hist_p *pt = &state->buf[n_tail];
		q16_t n_age = pg_dt_sec(&pt->ts, now);
		if (n_age < delay_sec)
			break;

		state->tail = n_tail;
		state->count -= 1;
	}

	*ndelay = y_nd;
	*delay = state->buf[state->tail].value;
}

q16_t pg_thermal_update(struct pg_thermal_state *RESTRICT state, q16_t cpu_temp,
			q16_t bat_temp, q16_t psi_load,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now)
{
	q16_t dt = pg_dt_sec(&state->last_tick, now);
	q16_t dt_s = pg_math_clamp(dt, FLOAT_TO_Q16(0.01F), Q16_ONE);
	state->last_tick = *now;

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

	q16_t u_ff = pg_thermal_lead_lag(&state->ff, psi_load, dt_s,
					 cfg->ff_gain, cfg->ff_lead_t,
					 cfg->ff_lag_t);

	q16_t u_cur = state->prev_sat;
	q16_t y_nd;
	q16_t y_d;
	pg_thermal_smith(&state->smith, u_cur, dt_s, cfg->smith_gain,
			 cfg->smith_tau, cfg->smith_delay, now, &y_nd, &y_d);

	q16_t e_pred = y_nd - y_d;
	q16_t adj_pv = cpu_temp + e_pred;
	q16_t err = adj_pv - sp;

	q16_t p_term = q16_mul(k_p, err);
	q16_t i_inc = q16_mul(q16_mul(k_i, err), dt_s);

	state->integ += i_inc;
	state->integ =
		pg_math_clamp(state->integ, INT_TO_Q16(-50), INT_TO_Q16(50));

	q16_t t_d = (k_p > 0) ? q16_div(k_d, k_p) : 0;
	q16_t n = cfg->filt_n;
	q16_t den = t_d + q16_mul(n, dt_s);

	q16_t d_term = 0;
	if (den > 0) {
		q16_t alpha = q16_div(t_d, den);
		q16_t beta = q16_div(q16_mul(k_d, n), den);
		q16_t d_pv = adj_pv - state->prev_pv;
		d_term = q16_mul(alpha, state->prev_d) + q16_mul(beta, d_pv);
	}

	state->prev_pv = adj_pv;
	state->prev_d = d_term;

	q16_t u_raw = p_term + state->integ + d_term + u_ff;
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
