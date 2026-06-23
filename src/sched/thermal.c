// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/thermal.h"
#include "pascal_gov/math.h"
#include "pascal_gov/time.h"
#include <math.h>

void pascal_gov_thermal_init(
	pascal_gov_thermal_state *PASCAL_GOV_RESTRICT state)
{
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);

	state->integral_accum = 0.0F;
	state->prev_adjusted_pv = 0.0F;
	state->prev_deriv_output = 0.0F;
	state->prev_output_sat = 0.0F;

	state->feedforward.prev_y = 0.0F;
	state->feedforward.prev_u = 0.0F;
	state->feedforward.first_run = true;

	state->smith_predictor.model_output_no_delay = 0.0F;
	state->smith_predictor.head = 0;
	state->smith_predictor.tail = 0;
	state->smith_predictor.count = 0;

	for (size_t i = 0; i < PASCAL_GOV_SMITH_BUFFER_SIZE; ++i) {
		state->smith_predictor.delay_buffer[i].value = 0.0F;
		state->smith_predictor.delay_buffer[i].timestamp =
			state->last_tick;
	}
}

static inline float pascal_gov_thermal_lead_lag_update(
	pascal_gov_lead_lag_state *PASCAL_GOV_RESTRICT state, float input,
	float dt, float gain, float t_lead, float t_lag)
{
	if (PASCAL_GOV_UNLIKELY(state->first_run)) {
		state->prev_u = input;
		state->prev_y = input * gain;
		state->first_run = false;
		return state->prev_y;
	}

	float denom = (2.0F * t_lag) + dt;
	float coeff_prev_y = (2.0F * t_lag) - dt;
	float coeff_input = gain * ((2.0F * t_lead) + dt);
	float coeff_prev_u = gain * ((2.0F * t_lead) - dt);

	float numerator = (coeff_input * input) -
			  (coeff_prev_u * state->prev_u) +
			  (coeff_prev_y * state->prev_y);

	float output = numerator / denom;

	state->prev_u = input;
	state->prev_y = output;

	return output;
}

static inline void pascal_gov_thermal_smith_update(
	pascal_gov_smith_predictor *PASCAL_GOV_RESTRICT state, float u_control,
	float dt, float k_gain, float tau, float delay_sec,
	const struct timespec *PASCAL_GOV_RESTRICT now,
	float *PASCAL_GOV_RESTRICT out_no_delay,
	float *PASCAL_GOV_RESTRICT out_delayed)
{
	float alpha = dt / (tau + dt);
	float y_no_delay = (alpha * (u_control * k_gain)) +
			   ((1.0F - alpha) * state->model_output_no_delay);

	state->model_output_no_delay = y_no_delay;

	if (state->count == PASCAL_GOV_SMITH_BUFFER_SIZE)
		state->tail =
			(state->tail + 1) & (PASCAL_GOV_SMITH_BUFFER_SIZE - 1);
	else
		state->count += 1;

	state->delay_buffer[state->head].value = y_no_delay;
	state->delay_buffer[state->head].timestamp = *now;

	state->head = (state->head + 1) & (PASCAL_GOV_SMITH_BUFFER_SIZE - 1);

	while (true) {
		size_t next_tail =
			(state->tail + 1) & (PASCAL_GOV_SMITH_BUFFER_SIZE - 1);

		if (next_tail == state->head)
			break;

		const pascal_gov_history_point *next_point =
			&state->delay_buffer[next_tail];

		float next_age = pascal_gov_dt_sec(&next_point->timestamp, now);
		if (next_age >= delay_sec) {
			state->tail = next_tail;
			state->count -= 1;
		} else {
			break;
		}
	}

	*out_no_delay = y_no_delay;
	*out_delayed = state->delay_buffer[state->tail].value;
}

float pascal_gov_thermal_update(
	pascal_gov_thermal_state *PASCAL_GOV_RESTRICT state, float cpu_temp,
	float bat_temp, float psi_load,
	const pascal_gov_thermal_config *PASCAL_GOV_RESTRICT tunables,
	const struct timespec *PASCAL_GOV_RESTRICT now)
{
	float dt = pascal_gov_dt_sec(&state->last_tick, now);
	float dt_safe = pascal_gov_clampf(dt, 0.01F, 1.0F);
	state->last_tick = *now;

	float sigma = pascal_gov_clampf(
		(bat_temp - tunables->sched_temp_cool) /
			(tunables->sched_temp_hot - tunables->sched_temp_cool),
		0.0F, 1.0F);

	float k_p = tunables->kp_base +
		    (sigma * (tunables->kp_fast - tunables->kp_base));

	float k_i = tunables->ki_base +
		    (sigma * (tunables->ki_fast - tunables->ki_base));

	float k_d = tunables->kd_base +
		    (sigma * (tunables->kd_fast - tunables->kd_base));

	float bat_margin = fmaxf(tunables->hard_limit_bat - bat_temp, 0.0F);
	float control_margin = (bat_margin < 5.0F) ? (5.0F - bat_margin) : 0.0F;

	float setpoint = tunables->hard_limit_cpu - control_margin;

	float u_ff = pascal_gov_thermal_lead_lag_update(
		&state->feedforward, psi_load, dt_safe, tunables->ff_gain,
		tunables->ff_lead_time, tunables->ff_lag_time);

	float current_control_effort = state->prev_output_sat;

	float y_pred_no_delay;
	float y_pred_delayed;

	pascal_gov_thermal_smith_update(
		&state->smith_predictor, current_control_effort, dt_safe,
		tunables->smith_gain, tunables->smith_tau,
		tunables->smith_delay_sec, now, &y_pred_no_delay,
		&y_pred_delayed);

	float pred_error_term = y_pred_no_delay - y_pred_delayed;
	float adjusted_pv = cpu_temp + pred_error_term;
	float error = adjusted_pv - setpoint;

	float p_term = k_p * error;
	float i_increment = k_i * error * dt_safe;
	state->integral_accum += i_increment;
	state->integral_accum =
		pascal_gov_clampf(state->integral_accum, -50.0F, 50.0F);

	float t_d = (k_p > 1e-6F) ? (k_d / k_p) : 0.0F;
	float n = tunables->deriv_filter_n;
	float denominator = t_d + (n * dt_safe);

	float d_term = 0.0F;
	if (denominator > 1e-6F) {
		float alpha = t_d / denominator;
		float beta = (k_d * n) / denominator;
		float delta_pv = adjusted_pv - state->prev_adjusted_pv;
		d_term = (alpha * state->prev_deriv_output) + (beta * delta_pv);
	}

	state->prev_adjusted_pv = adjusted_pv;
	state->prev_deriv_output = d_term;

	float u_raw = p_term + state->integral_accum + d_term + u_ff;
	float u_sat = pascal_gov_clampf(u_raw, 0.0F, 100.0F);

	if (fabsf(u_raw - u_sat) > 0.001F) {
		float excess = u_raw - u_sat;
		state->integral_accum -=
			excess * tunables->anti_windup_k * dt_safe;
	}

	state->prev_output_sat = u_sat;

	float pid_saturation = u_sat / 100.0F;
	float final_scale = 1.0F - pid_saturation;

	if (PASCAL_GOV_UNLIKELY(bat_temp >= tunables->hard_limit_bat))
		return fminf(final_scale, 0.2F);

	return pascal_gov_clampf(final_scale, 0.1F, 1.0F);
}
