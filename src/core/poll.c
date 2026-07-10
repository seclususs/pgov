// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/poll.h"
#include "pg/config.h"
#include "pg/time.h"

void pg_poll_init(struct pg_poll_state *RESTRICT state)
{
	state->cur_ivl = PG_MIN_POLL_MS;
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);

	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	state->rng_state =
		((uint64_t)now.tv_sec * 1000000000ULL) + (uint64_t)now.tv_nsec;
}

static inline uint64_t next_random(struct pg_poll_state *state, uint64_t range)
{
	if (UNLIKELY(range == 0))
		return 0;

#if defined(__SIZEOF_INT128__)
	state->rng_state = (state->rng_state * 6364136223846793005ULL) + 1ULL;

	uint64_t limit = range * 2;
	uint64_t lim_p1 = limit + 1;

	unsigned __int128 p =
		(unsigned __int128)state->rng_state * (unsigned __int128)lim_p1;

	return (uint64_t)(p >> 64);
#else
	uint64_t x = state->rng_state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	state->rng_state = x;

	return x % range;
#endif
}

static inline uint64_t jitter(struct pg_poll_state *state, uint64_t ivl,
			      uint64_t min, uint64_t max)
{
	uint64_t step = 50;
	uint64_t clamp = ((ivl + (step / 2)) / step) * step;

	if (clamp < min)
		clamp = min;

	if (clamp > max)
		clamp = max;

	uint64_t n_amp = (clamp * 5) / 100;
	if (UNLIKELY(n_amp == 0))
		return clamp;

	uint64_t noise = next_random(state, n_amp);
	uint64_t final;
	if (noise > n_amp) {
		final = clamp + (noise - n_amp);
	} else {
		uint64_t sub = n_amp - noise;
		final = (clamp > sub) ? (clamp - sub) : 0;
	}

	if (final < min)
		final = min;

	if (final > max)
		final = max;

	return final;
}

uint64_t pg_poll_calc_next(struct pg_poll_state *state, q16_t cur_press,
			   q16_t avg300, q16_t press_vel)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	q16_t elapsed = pg_dt_sec(&state->last_tick, &now);
	uint64_t ms = (uint64_t)((((q32_t)elapsed) * 1000) >> Q16_SHIFT);
	if (UNLIKELY(ms > (state->cur_ivl + 200))) {
		state->last_tick = now;
		state->cur_ivl = PG_MIN_POLL_MS;
		return PG_MIN_POLL_MS;
	}

	q16_t pred = cur_press + q16_mul(press_vel, Q16_HALF);
	uint64_t nxt_ivl = state->cur_ivl;

	if (pred > INT_TO_Q16(5) || press_vel > INT_TO_Q16(2))
		nxt_ivl = PG_MIN_POLL_MS;
	else if (pred > FLOAT_TO_Q16(1.5F))
		nxt_ivl /= 2;
	else if (avg300 < INT_TO_Q16(2))
		nxt_ivl += 200;

	if (nxt_ivl < PG_MIN_POLL_MS)
		nxt_ivl = PG_MIN_POLL_MS;
	else if (nxt_ivl > PG_MAX_POLL_MS)
		nxt_ivl = PG_MAX_POLL_MS;

	state->last_tick = now;

	uint64_t diff = (nxt_ivl > state->cur_ivl) ?
				(nxt_ivl - state->cur_ivl) :
				(state->cur_ivl - nxt_ivl);
	if (diff >= 200)
		state->cur_ivl = nxt_ivl;

	return jitter(state, state->cur_ivl, PG_MIN_POLL_MS, PG_MAX_POLL_MS);
}
