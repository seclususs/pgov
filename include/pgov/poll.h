// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_POLL_H
#define PGOV_POLL_H

#include "pgov/math.h"
#include <stdint.h>
#include <time.h>

#define POLL_MIN_MS 100
#define POLL_MAX_MS 10000

struct poll_state {
	uint32_t cur_interval;
	uint32_t last_wait;
	struct timespec last_tick;
	uint64_t range;
};

void poll_init(struct poll_state *state);

/*
 * Calculate adaptive next sleep interval in milliseconds based on current
 * system pressure, 300s moving average, and pressure trend velocity
 */
uint64_t poll_calc_next(struct poll_state *state, q16_t cur_pressure,
			q16_t avg300, q16_t pressure_velocity);

#endif // PGOV_POLL_H
