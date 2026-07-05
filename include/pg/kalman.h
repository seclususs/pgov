// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_KALMAN_H
#define PG_KALMAN_H

#include "compiler.h"
#include <stdbool.h>

struct pg_kalman_cfg {
	float q_pos;
	float q_vel;
	float r_meas;
	float fade_fact;
};

struct pg_kalman_state {
	float x_pos;
	float x_vel;
	float p00;
	float p01;
	float p10;
	float p11;
	struct pg_kalman_cfg cfg;
	float nis;
	bool first_run;
};

void pg_kalman_init(struct pg_kalman_state *RESTRICT state,
		    const struct pg_kalman_cfg *RESTRICT cfg);

void pg_kalman_reset(struct pg_kalman_state *state);

float pg_kalman_update(struct pg_kalman_state *state, float z_meas,
		       float dt_sec);

#endif // PG_KALMAN_H
