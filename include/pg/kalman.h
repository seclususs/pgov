// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_KALMAN_H
#define PG_KALMAN_H

#include "compiler.h"
#include "pg/math.h"
#include <stdbool.h>

struct pg_kalman_state {
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

void pg_kalman_init(struct pg_kalman_state *RESTRICT state);

void pg_kalman_reset(struct pg_kalman_state *state);

q16_t pg_kalman_update(struct pg_kalman_state *state, q16_t z_meas,
		       q16_t dt_sec);

#endif // PG_KALMAN_H
