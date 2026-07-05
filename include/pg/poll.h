// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_POLL_H
#define PG_POLL_H

#include "compiler.h"
#include <stdint.h>
#include <time.h>

struct pg_poll_cfg {
	uint64_t sleep_tol;
	uint64_t quant;
	uint64_t hyst;
	uint64_t noise_pct;
	float rise_fact;
	float fall_fact;
};

struct pg_poll_state {
	uint64_t cur_ivl;
	struct timespec last_tick;
	uint64_t tgt_ivl;
	float press_wt;
	float deriv_wt;
	uint64_t rng_state;
	struct pg_poll_cfg cfg;
};

void pg_poll_init(struct pg_poll_state *RESTRICT state, float press_wt,
		  float deriv_wt, const struct pg_poll_cfg *RESTRICT cfg);

uint64_t pg_poll_calc_next(struct pg_poll_state *state, float cur_press,
			   float avg300, float press_vel);

#endif // PG_POLL_H
