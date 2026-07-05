// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/poll.h"
#include "pg/config.h"
#include "pg/math.h"
#include "pg/time.h"
#include <math.h>

void pg_poll_init(struct pg_poll_state *RESTRICT state, float press_wt,
		  float deriv_wt, const struct pg_poll_cfg *RESTRICT cfg)
{
	state->cur_ivl = PG_MIN_POLL_MS;
	clock_gettime(CLOCK_MONOTONIC, &state->last_tick);
	state->tgt_ivl = PG_MIN_POLL_MS;
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

	state->rng_state = (state->rng_state * 6364136223846793005ULL) + 1ULL;

	uint64_t limit = range * 2;
	uint64_t limit_plus_one = limit + 1;

	unsigned __int128 product = (unsigned __int128)state->rng_state *
				    (unsigned __int128)limit_plus_one;

	return (uint64_t)(product >> 64);
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

uint64_t pg_poll_calc_next(struct pg_poll_state *state, float cur_press,
			   float avg300, float press_vel)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	float elapsed = pg_dt_sec(&state->last_tick, &now);
	uint64_t ms = (uint64_t)(elapsed * 1000.0F);
	if (UNLIKELY(ms > (state->cur_ivl + state->cfg.sleep_tol))) {
		state->last_tick = now;
		state->cur_ivl = PG_MIN_POLL_MS;
		return PG_MIN_POLL_MS;
	}

	uint64_t min;
	uint64_t max;
	if (avg300 < 2.0F && cur_press < 10.0F) {
		min = 6000;
		max = PG_MAX_POLL_MS;
	} else if (avg300 > 20.0F) {
		min = PG_MIN_POLL_MS;
		max = 5000;
	} else {
		min = PG_MIN_POLL_MS;
		max = PG_MAX_POLL_MS;
	}

	float dv = press_vel;
	float pred = cur_press + (dv * 0.5F);

	float p_term = pred * state->press_wt;
	float d_term = fabsf(dv) * state->deriv_wt;

	float score = pg_math_clampf(p_term + d_term, 0.0F, 100.0F);
	float range = (float)(max - min);
	float raw_ivl = (float)max - ((score / 100.0F) * range);

	float target = (float)state->tgt_ivl;
	float next;

	if (raw_ivl < target) {
		float alpha = state->cfg.fall_fact;
		next = (alpha * raw_ivl) + ((1.0F - alpha) * target);
	} else {
		float alpha = state->cfg.rise_fact;
		next = (alpha * raw_ivl) + ((1.0F - alpha) * target);
	}

	state->tgt_ivl = (uint64_t)next;
	state->last_tick = now;

	uint64_t diff = (state->tgt_ivl > state->cur_ivl) ?
				(state->tgt_ivl - state->cur_ivl) :
				(state->cur_ivl - state->tgt_ivl);

	if (diff < state->cfg.hyst)
		return poll_discrete(state, state->cur_ivl, min, max);

	state->cur_ivl = state->tgt_ivl;
	return poll_discrete(state, state->cur_ivl, min, max);
}
