// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_FILTER_H
#define PASCAL_GOV_FILTER_H

#include "pascal_gov/compiler.h"
#include <stdbool.h>

typedef struct {
	float q_pos;
	float q_vel;
	float r_meas;
	float fading_factor;
} pascal_gov_kalman_config;

typedef struct {
	float x_pos;
	float x_vel;
	float p00;
	float p01;
	float p10;
	float p11;
	pascal_gov_kalman_config config;
	float last_nis;
	bool first_run;
} pascal_gov_kalman_state;

void pascal_gov_kalman_init(
	pascal_gov_kalman_state *PASCAL_GOV_RESTRICT state,
	const pascal_gov_kalman_config *PASCAL_GOV_RESTRICT config);

void pascal_gov_kalman_reset(pascal_gov_kalman_state *state);

float pascal_gov_kalman_update(pascal_gov_kalman_state *state, float z_meas,
			       float dt_sec);

#endif // PASCAL_GOV_FILTER_H
