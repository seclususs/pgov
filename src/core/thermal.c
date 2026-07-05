// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/thermal.h"
#include "pg/math.h"
#include "pg/time.h"
#include <math.h>

void pg_thermal_init(struct pg_thermal_state *state)
{
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);

	state->integ = 0.0F;
	state->prev_pv = 0.0F;
	state->prev_d = 0.0F;
	state->prev_sat = 0.0F;

	state->ff.prev_y = 0.0F;
	state->ff.prev_u = 0.0F;
	state->ff.first_run = true;

	state->smith.ndelay = 0.0F;
	state->smith.head = 0;
	state->smith.tail = 0;
	state->smith.count = 0;

	for (size_t i = 0; i < SMITH_BUFFER; ++i) {
		state->smith.buf[i].value = 0.0F;
		state->smith.buf[i].ts = state->last_tick;
	}
}

static inline float pg_thermal_lead_lag(struct pg_lead_lag_state *state,
					float input, float dt, float gain,
					float t_lead, float t_lag)
{
	if (UNLIKELY(state->first_run)) {
		state->prev_u = input;
		state->prev_y = input * gain;
		state->first_run = false;
		return state->prev_y;
	}

	float denom = (2.0F * t_lag) + dt;
	float c_py = (2.0F * t_lag) - dt;
	float c_in = gain * ((2.0F * t_lead) + dt);
	float c_pu = gain * ((2.0F * t_lead) - dt);
	float num = (c_in * input) - (c_pu * state->prev_u) +
		    (c_py * state->prev_y);

	float output = num / denom;

	state->prev_u = input;
	state->prev_y = output;

	return output;
}

static inline void pg_thermal_smith(struct pg_smith_pred *RESTRICT state,
				    float u_control, float dt, float k_gain,
				    float tau, float delay_sec,
				    const struct timespec *RESTRICT now,
				    float *RESTRICT ndelay,
				    float *RESTRICT delay)
{
	float alpha = dt / (tau + dt);
	float y_nd = (alpha * (u_control * k_gain)) +
		     ((1.0F - alpha) * state->ndelay);

	state->ndelay = y_nd;
	if (state->count == SMITH_BUFFER)
		state->tail = (state->tail + 1) & (SMITH_BUFFER - 1);
	else
		state->count += 1;

	state->buf[state->head].value = y_nd;
	state->buf[state->head].ts = *now;
	state->head = (state->head + 1) & (SMITH_BUFFER - 1);

	while (true) {
		size_t next_tail = (state->tail + 1) & (SMITH_BUFFER - 1);
		if (next_tail == state->head)
			break;

		const struct pg_hist_p *pt = &state->buf[next_tail];

		float next_age = pg_dt_sec(&pt->ts, now);
		if (next_age >= delay_sec) {
			state->tail = next_tail;
			state->count -= 1;
		} else {
			break;
		}
	}

	*ndelay = y_nd;
	*delay = state->buf[state->tail].value;
}

float pg_thermal_update(struct pg_thermal_state *RESTRICT state, float cpu_temp,
			float bat_temp, float psi_load,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now)
{
	float dt = pg_dt_sec(&state->last_tick, now);
	float dt_safe = pg_math_clampf(dt, 0.01F, 1.0F);
	state->last_tick = *now;

	float sigma = pg_math_clampf((bat_temp - cfg->temp_cool) /
					     (cfg->temp_hot - cfg->temp_cool),
				     0.0F, 1.0F);

	float k_p = cfg->kp_base + (sigma * (cfg->kp_fast - cfg->kp_base));
	float k_i = cfg->ki_base + (sigma * (cfg->ki_fast - cfg->ki_base));
	float k_d = cfg->kd_base + (sigma * (cfg->kd_fast - cfg->kd_base));

	float bat_marg = fmaxf(cfg->limit_bat - bat_temp, 0.0F);
	float ctrl_marg = (bat_marg < 5.0F) ? (5.0F - bat_marg) : 0.0F;
	float setpoint = cfg->limit_cpu - ctrl_marg;

	float u_ff = pg_thermal_lead_lag(&state->ff, psi_load, dt_safe,
					 cfg->ff_gain, cfg->ff_lead_t,
					 cfg->ff_lag_t);

	float u_cur = state->prev_sat;

	float y_pred_no_delay;
	float y_pred_delayed;
	pg_thermal_smith(&state->smith, u_cur, dt_safe, cfg->smith_gain,
			 cfg->smith_tau, cfg->smith_delay, now,
			 &y_pred_no_delay, &y_pred_delayed);

	float err_pred = y_pred_no_delay - y_pred_delayed;
	float adj_pv = cpu_temp + err_pred;
	float error = adj_pv - setpoint;
	float p_term = k_p * error;
	float i_inc = k_i * error * dt_safe;
	state->integ += i_inc;
	state->integ = pg_math_clampf(state->integ, -50.0F, 50.0F);

	float t_d = (k_p > 1e-6F) ? (k_d / k_p) : 0.0F;
	float n = cfg->filt_n;
	float den = t_d + (n * dt_safe);

	float d_term = 0.0F;
	if (den > 1e-6F) {
		float alpha = t_d / den;
		float beta = (k_d * n) / den;
		float delta_pv = adj_pv - state->prev_pv;
		d_term = (alpha * state->prev_d) + (beta * delta_pv);
	}

	state->prev_pv = adj_pv;
	state->prev_d = d_term;

	float u_raw = p_term + state->integ + d_term + u_ff;
	float u_sat = pg_math_clampf(u_raw, 0.0F, 100.0F);
	if (fabsf(u_raw - u_sat) > 0.001F) {
		float excess = u_raw - u_sat;
		state->integ -= excess * cfg->aw_k * dt_safe;
	}

	state->prev_sat = u_sat;
	float pid_sat = u_sat / 100.0F;
	float final_scale = 1.0F - pid_sat;
	if (UNLIKELY(bat_temp >= cfg->limit_bat))
		return fminf(final_scale, 0.2F);

	return pg_math_clampf(final_scale, 0.1F, 1.0F);
}
