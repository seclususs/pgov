// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/poll.h"
#include "pgov/compiler.h"
#include "pgov/time.h"

void poll_init(struct poll_state *state)
{
	struct timespec now;

	if (UNLIKELY(!state))
		return;

	state->cur_interval = POLL_MIN_MS;
	state->last_wait = POLL_MIN_MS;
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);

	clock_gettime(CLOCK_REALTIME, &now);
	state->range =
		((uint64_t)now.tv_sec * 1000000000ULL) + (uint64_t)now.tv_nsec;
}

static inline uint64_t next_random(struct poll_state *state, uint64_t range)
{
	if (UNLIKELY(!state || range == 0))
		return 0;

#ifdef __SIZEOF_INT128__
	unsigned __int128 p;

	state->range = (state->range * 6364136223846793005ULL) + 1ULL;
	p = (unsigned __int128)state->range * (unsigned __int128)range;

	return (uint64_t)(p >> 64);
#else
	uint64_t x;

	x = state->range;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	state->range = x;

	return x % range;
#endif
}

static inline uint64_t jitter(struct poll_state *state, uint64_t interval,
			      uint64_t min, uint64_t max)
{
	uint64_t clamp;
	uint64_t final;
	uint64_t n_amp;
	uint64_t noise;
	uint64_t step = 50;
	uint64_t sub;

	if (UNLIKELY(!state))
		return interval;

	clamp = ((interval + (step / 2)) / step) * step;

	if (clamp < min)
		clamp = min;

	if (clamp > max)
		clamp = max;

	n_amp = (clamp * 5) / 100;
	if (UNLIKELY(n_amp == 0))
		return clamp;

	noise = next_random(state, (n_amp * 2) + 1);
	if (noise > n_amp) {
		final = clamp + (noise - n_amp);
	} else {
		sub = n_amp - noise;
		final = (clamp > sub) ? (clamp - sub) : 0;
	}

	if (final < min)
		final = min;

	if (final > max)
		final = max;

	return final;
}

uint64_t poll_calc_next(struct poll_state *state, q16_t cur_pressure,
			q16_t avg300, q16_t pressure_velocity)
{
	struct timespec now;
	uint64_t diff;
	q16_t elapsed;
	uint64_t ms;
	uint64_t interval;
	q16_t pred;
	uint64_t wait;

	if (UNLIKELY(!state))
		return POLL_MIN_MS;

	clock_gettime(CLOCK_MONOTONIC, &now);

	elapsed = time_dt_sec(&state->last_tick, &now);
	if (UNLIKELY(elapsed < 0))
		elapsed = 0;

	ms = (uint64_t)((((q32_t)elapsed) * 1000) >> Q16_SHIFT);
	if (UNLIKELY(ms > ((uint64_t)state->last_wait + 200))) {
		state->last_tick = now;
		state->cur_interval = POLL_MIN_MS;
		state->last_wait = POLL_MIN_MS;
		return POLL_MIN_MS;
	}

	pred = cur_pressure + math_mul_q16(pressure_velocity, Q16_HALF);
	interval = state->cur_interval;

	if (pred > INT_TO_Q16(5) || pressure_velocity > INT_TO_Q16(2))
		interval = POLL_MIN_MS;

	else if (pred > FLOAT_TO_Q16(1.5F))
		interval /= 2;

	else if (avg300 < INT_TO_Q16(2))
		interval += 200;

	if (interval < POLL_MIN_MS)
		interval = POLL_MIN_MS;

	else if (interval > POLL_MAX_MS)
		interval = POLL_MAX_MS;

	state->last_tick = now;

	diff = (interval > state->cur_interval) ?
		       (interval - state->cur_interval) :
		       (state->cur_interval - interval);

	if (interval < state->cur_interval || diff >= 200)
		state->cur_interval = (uint32_t)interval;

	wait = jitter(state, state->cur_interval, POLL_MIN_MS, POLL_MAX_MS);
	state->last_wait = (uint32_t)wait;
	return wait;
}
