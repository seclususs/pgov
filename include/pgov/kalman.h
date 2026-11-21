// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_KALMAN_H
#define PGOV_KALMAN_H

#include "pgov/math.h"
#include <stdbool.h>

/*
 * Fixed-point Q16/Q32 Kalman filter tracking position, velocity, and
 * adaptive innovation noise for PSI pressure and temperature signals
 */
struct kalman_state {
	q16_t x_pos;
	q16_t x_vel;
	q32_t p00;
	q32_t p01;
	q32_t p10;
	q32_t p11;
	q16_t q_vel;
	q16_t r_meas;
	q32_t cov_y;
	q16_t nis;
	bool first_run;
};

void kalman_init(struct kalman_state *state);

void kalman_reset(struct kalman_state *state);

/*
 * Advance filter prediction by dt_sec and update with meas
 *
 * Returns the smoothed position estimate floored at zero
 */
q16_t kalman_update(struct kalman_state *state, q16_t meas, q16_t dt_sec);

#endif // PGOV_KALMAN_H
