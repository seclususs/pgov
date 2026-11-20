// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_THERMAL_H
#define PGOV_THERMAL_H

#include "pgov/math.h"
#include "pgov/kalman.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

struct thermal_param {
	q16_t limit_cpu;
	q16_t limit_bat;
	q16_t kp_base;
	q16_t ki_base;
	q16_t kd_base;
};

struct thermal_state {
	struct timespec last_tick;
	q16_t integral;
	q16_t prev_sat;
	int32_t sat_count;
	struct kalman_state filter;
};

void thermal_init(struct thermal_state *state);

q16_t thermal_update(struct thermal_state *state, q16_t cpu_temp,
		     q16_t bat_temp, const struct thermal_param *param,
		     const struct timespec *now);

#endif // PGOV_THERMAL_H
