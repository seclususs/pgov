// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/poll.h"
#include "pg/config.h"
#include "pg/time.h"

void pg_poll_init(struct pg_poll_state *RESTRICT state, q16_t press_wt,
		  q16_t deriv_wt, const struct pg_poll_cfg *RESTRICT cfg)
{
	state->cur_ivl = PG_MIN_POLL_MS;
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->tgt_ivl = INT_TO_Q16(PG_MIN_POLL_MS);
	state->press_wt = press_wt;
	state->deriv_wt = deriv_wt;
	state->cfg = *cfg;

	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	state->rng_state =
		((uint64_t)now.tv_sec * 1000000000ULL) + (uint64_t)now.tv_nsec;
}

static inline uint64_t poll_next_random(struct pg_poll_state *state,
					uint64_t range)
{
	if (UNLIKELY(range == 0))
		return 0;

#if defined(__SIZEOF_INT128__)
	state->rng_state = (state->rng_state * 6364136223846793005ULL) + 1ULL;

	uint64_t limit = range * 2;
	uint64_t limit_plus_one = limit + 1;

	unsigned __int128 product = (unsigned __int128)state->rng_state *
				    (unsigned __int128)limit_plus_one;

	return (uint64_t)(product >> 64);
#else
	uint64_t x = state->rng_state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	state->rng_state = x;

	return x % range;
#endif
}

static inline uint64_t poll_discrete(struct pg_poll_state *state, uint64_t ivl,
				     uint64_t min, uint64_t max)
{
	uint64_t step = state->cfg.quant;
	uint64_t quant = ((ivl + (step / 2)) / step) * step;
	uint64_t clamp = quant;

	if (clamp < min)
		clamp = min;

	if (clamp > max)
		clamp = max;

	uint64_t noise_ampl = (clamp * state->cfg.noise_pct) / 100;
	if (UNLIKELY(noise_ampl == 0))
		return clamp;

	uint64_t noise = poll_next_random(state, noise_ampl);
	uint64_t final;
	if (noise > noise_ampl)
		final = clamp + (noise - noise_ampl);
	else {
		uint64_t sub = noise_ampl - noise;
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
	if (UNLIKELY(ms > (state->cur_ivl + state->cfg.sleep_tol))) {
		state->last_tick = now;
		state->cur_ivl = PG_MIN_POLL_MS;
		return PG_MIN_POLL_MS;
	}

	uint64_t min;
	uint64_t max;
	if (avg300 < INT_TO_Q16(2) && cur_press < INT_TO_Q16(10)) {
		min = 6000;
		max = PG_MAX_POLL_MS;
	} else if (avg300 > INT_TO_Q16(20)) {
		min = PG_MIN_POLL_MS;
		max = 5000;
	} else {
		min = PG_MIN_POLL_MS;
		max = PG_MAX_POLL_MS;
	}

	q16_t dv = press_vel;
	q16_t pred = cur_press + q16_mul(dv, Q16_HALF);
	q16_t p_term = q16_mul(pred, state->press_wt);
	q16_t d_term = q16_mul(ABS_Q16(dv), state->deriv_wt);
	q16_t score = pg_math_clamp(p_term + d_term, 0, INT_TO_Q16(100));

	q16_t range = INT_TO_Q16(max - min);
	q16_t raw_ivl = INT_TO_Q16(max) -
			q16_mul(q16_div(score, INT_TO_Q16(100)), range);

	q16_t target = state->tgt_ivl;
	q16_t next;
	if (raw_ivl < target) {
		q16_t alpha = state->cfg.fall_fact;
		next = q16_mul(alpha, raw_ivl) +
		       q16_mul(Q16_ONE - alpha, target);
	} else {
		q16_t alpha = state->cfg.rise_fact;
		next = q16_mul(alpha, raw_ivl) +
		       q16_mul(Q16_ONE - alpha, target);
	}

	state->tgt_ivl = next;
	state->last_tick = now;

	uint64_t tgt_ms = (uint64_t)Q16_TO_INT(state->tgt_ivl);
	uint64_t diff = (tgt_ms > state->cur_ivl) ? (tgt_ms - state->cur_ivl) :
						    (state->cur_ivl - tgt_ms);

	if (diff < state->cfg.hyst)
		return poll_discrete(state, state->cur_ivl, min, max);

	state->cur_ivl = tgt_ms;
	return poll_discrete(state, state->cur_ivl, min, max);
}
