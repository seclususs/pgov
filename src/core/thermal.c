// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/thermal.h"
#include "pgov/time.h"

#define STALE_SEC 30

static void reset_filter(struct thermal_state *state)
{
	pg_kalman_init(&state->filter);
	state->filter.q_vel = FLOAT_TO_Q16(0.01F);
}

void thermal_init(struct thermal_state *state)
{
	if (UNLIKELY(!state))
		return;

	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->integral = 0;
	state->prev_sat = 0;
	state->sat_count = 0;
	reset_filter(state);
}

q16_t thermal_update(struct thermal_state *state, q16_t cpu_temp,
		     q16_t bat_temp, const struct thermal_param *param,
		     const struct timespec *now)
{
	q16_t aw_boost;
	q16_t aw_gain;
	q16_t bat_margin;
	q16_t cpu_margin;
	q16_t dist;
	q16_t drain;
	q16_t dt;
	q16_t dt_safe;
	q16_t err;
	q16_t excess;
	q16_t kd;
	q16_t kd_boost;
	q16_t ki;
	q16_t kp;
	q16_t scale;
	q16_t setpoint;
	q16_t temp;
	q16_t u_raw;
	q16_t u_sat;
	q16_t vel;

	if (UNLIKELY(!state || !param || !now))
		return Q16_ONE;

	dt = pg_dt_sec(&state->last_tick, now);
	dt_safe = math_clamp(dt, FLOAT_TO_Q16(0.01F), Q16_ONE);
	state->last_tick = *now;

	bat_margin = param->limit_bat - bat_temp;
	if (bat_margin < 0)
		bat_margin = 0;

	cpu_margin =
		(bat_margin < INT_TO_Q16(5)) ? (INT_TO_Q16(5) - bat_margin) : 0;
	setpoint = param->limit_cpu - cpu_margin;

	if (UNLIKELY(dt > INT_TO_Q16(STALE_SEC))) {
		reset_filter(state);

		state->prev_sat = 0;
		state->sat_count = 0;

		if (cpu_temp < setpoint)
			state->integral = 0;
	}

	temp = pg_kalman_update(&state->filter, cpu_temp, dt_safe);
	vel = state->filter.x_vel;
	err = temp - setpoint;

	dist = setpoint - temp;
	if (dist < FLOAT_TO_Q16(0.1F))
		dist = FLOAT_TO_Q16(0.1F);

	kp = param->kp_base + math_div_q16(param->kp_base, dist);
	ki = param->ki_base + math_div_q16(param->ki_base, dist);
	kd_boost = (vel > 0) ? math_mul_q16(FLOAT_TO_Q16(2.0F), vel) : 0;
	kd = param->kd_base + kd_boost;

	state->integral += math_mul_q16(math_mul_q16(ki, err), dt_safe);
	state->integral =
		math_clamp(state->integral, INT_TO_Q16(-50), INT_TO_Q16(50));

	u_raw = math_mul_q16(kp, err) + state->integral + math_mul_q16(kd, vel);
	u_sat = math_clamp(u_raw, 0, INT_TO_Q16(100));

	if (state->prev_sat >= INT_TO_Q16(100)) {
		if (state->sat_count < 50)
			state->sat_count++;
	} else {
		state->sat_count = 0;
	}

	aw_gain = FLOAT_TO_Q16(1.95F);
	if (state->sat_count > 0) {
		aw_boost = math_mul_q16(FLOAT_TO_Q16(0.1F),
					INT_TO_Q16(state->sat_count));

		aw_gain += aw_boost;
	}

	if (math_abs_q16(u_raw - u_sat) > 65) {
		excess = u_raw - u_sat;
		drain = math_mul_q16(math_mul_q16(excess, aw_gain), dt_safe);

		if ((state->integral > 0 && drain > state->integral) ||
		    (state->integral < 0 && drain < state->integral))
			state->integral = 0;
		else
			state->integral -= drain;
	}

	state->prev_sat = u_sat;
	scale = Q16_ONE - math_div_q16(u_sat, INT_TO_Q16(100));

	if (UNLIKELY(bat_temp >= param->limit_bat))
		return math_clamp(scale, FLOAT_TO_Q16(0.1F),
				  FLOAT_TO_Q16(0.2F));

	return math_clamp(scale, FLOAT_TO_Q16(0.1F), Q16_ONE);
}
