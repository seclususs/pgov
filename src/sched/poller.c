// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/poller.h"
#include "daemon/config.h"
#include "pascal_gov/math.h"
#include "pascal_gov/time.h"
#include <math.h>

void pascal_gov_poller_init(
	pascal_gov_poller_state *PASCAL_GOV_RESTRICT state,
	float weight_pressure, float weight_derivative,
	const pascal_gov_poller_config *PASCAL_GOV_RESTRICT tunables)
{
	state->current_interval = PASCAL_GOV_MIN_POLLING_MS;
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->target_interval = PASCAL_GOV_MIN_POLLING_MS;
	state->weight_pressure = weight_pressure;
	state->weight_derivative = weight_derivative;
	state->tunables = *tunables;

	struct timespec real_now;
	clock_gettime(CLOCK_REALTIME, &real_now);
	state->rng_state = (uint64_t)real_now.tv_sec * 1000000000ULL +
			   (uint64_t)real_now.tv_nsec;
}

static inline uint64_t pascal_gov_poller_next_random(
	pascal_gov_poller_state *PASCAL_GOV_RESTRICT state, uint64_t range)
{
	if (PASCAL_GOV_UNLIKELY(range == 0))
		return 0;

	state->rng_state = state->rng_state * 6364136223846793005ULL + 1ULL;

	uint64_t limit = range * 2;
	uint64_t limit_plus_one = limit + 1;

	unsigned __int128 product = (unsigned __int128)state->rng_state *
				    (unsigned __int128)limit_plus_one;

	return (uint64_t)(product >> 64);
}

static inline uint64_t pascal_gov_poller_apply_discrete_math(
	pascal_gov_poller_state *PASCAL_GOV_RESTRICT state, uint64_t interval,
	uint64_t min_limit, uint64_t max_limit)
{
	uint64_t step = state->tunables.quantization_step_ms;
	uint64_t quantized = ((interval + (step / 2)) / step) * step;
	uint64_t clamped = quantized;

	if (clamped < min_limit)
		clamped = min_limit;

	if (clamped > max_limit)
		clamped = max_limit;

	uint64_t noise_amplitude =
		(clamped * state->tunables.noise_percent) / 100;

	if (PASCAL_GOV_UNLIKELY(noise_amplitude == 0))
		return clamped;

	uint64_t noise_val =
		pascal_gov_poller_next_random(state, noise_amplitude);

	uint64_t final_val;

	if (noise_val > noise_amplitude) {
		final_val = clamped + (noise_val - noise_amplitude);
	} else {
		uint64_t sub_val = noise_amplitude - noise_val;
		final_val = (clamped > sub_val) ? (clamped - sub_val) : 0;
	}

	if (final_val < min_limit)
		final_val = min_limit;

	if (final_val > max_limit)
		final_val = max_limit;

	return final_val;
}

uint64_t pascal_gov_poller_calculate_next_interval(
	pascal_gov_poller_state *PASCAL_GOV_RESTRICT state,
	float current_pressure, float avg300, float pressure_velocity)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	float elapsed_sec = pascal_gov_dt_sec(&state->last_tick, &now);
	uint64_t elapsed_ms = (uint64_t)(elapsed_sec * 1000.0F);

	if (PASCAL_GOV_UNLIKELY(elapsed_ms >
				(state->current_interval +
				 state->tunables.sleep_tolerance_ms))) {
		state->last_tick = now;
		state->current_interval = PASCAL_GOV_MIN_POLLING_MS;
		return PASCAL_GOV_MIN_POLLING_MS;
	}

	uint64_t dynamic_min;
	uint64_t dynamic_max;

	if (avg300 < 2.0F && current_pressure < 10.0F) {
		dynamic_min = 6000;
		dynamic_max = PASCAL_GOV_MAX_POLLING_MS;
	} else if (avg300 > 20.0F) {
		dynamic_min = PASCAL_GOV_MIN_POLLING_MS;
		dynamic_max = 5000;
	} else {
		dynamic_min = PASCAL_GOV_MIN_POLLING_MS;
		dynamic_max = PASCAL_GOV_MAX_POLLING_MS;
	}

	float rate_change = pressure_velocity;
	float prediction = current_pressure + (rate_change * 0.5F);

	float p_term = prediction * state->weight_pressure;
	float d_term = fabsf(rate_change) * state->weight_derivative;

	float priority_score = pascal_gov_clampf(p_term + d_term, 0.0F, 100.0F);
	float interval_range = (float)(dynamic_max - dynamic_min);
	float raw_interval = (float)dynamic_max -
			     ((priority_score / 100.0F) * interval_range);

	float target_f32 = (float)state->target_interval;
	float next_target;

	if (raw_interval < target_f32) {
		float alpha = state->tunables.fall_factor;
		next_target =
			(alpha * raw_interval) + ((1.0F - alpha) * target_f32);
	} else {
		float alpha = state->tunables.rise_factor;
		next_target =
			(alpha * raw_interval) + ((1.0F - alpha) * target_f32);
	}

	state->target_interval = (uint64_t)next_target;
	state->last_tick = now;

	uint64_t diff =
		(state->target_interval > state->current_interval)
			? (state->target_interval - state->current_interval)
			: (state->current_interval - state->target_interval);

	if (diff < state->tunables.hysteresis_threshold_ms)
		return pascal_gov_poller_apply_discrete_math(
			state, state->current_interval, dynamic_min,
			dynamic_max);

	state->current_interval = state->target_interval;

	return pascal_gov_poller_apply_discrete_math(
		state, state->current_interval, dynamic_min, dynamic_max);
}
