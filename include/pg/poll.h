// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_POLL_H
#define PG_POLL_H

#include "compiler.h"
#include "pg/math.h"
#include <stdint.h>
#include <time.h>

struct pg_poll_state {
	uint64_t cur_ivl;
	struct timespec last_tick;
	uint64_t rng_state;
};

void pg_poll_init(struct pg_poll_state *RESTRICT state);

uint64_t pg_poll_calc_next(struct pg_poll_state *state, q16_t cur_press,
			   q16_t avg300, q16_t press_vel);

#endif // PG_POLL_H
