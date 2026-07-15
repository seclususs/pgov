// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_THERMAL_H
#define PG_THERMAL_H

#include "compiler.h"
#include "pg/math.h"
#include "pg/kalman.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

struct pg_thermal_cfg {
	q16_t limit_cpu;
	q16_t limit_bat;
	q16_t kp_base;
	q16_t ki_base;
	q16_t kd_base;
};

struct pg_thermal_state {
	struct timespec last_tick;
	q16_t integ;
	q16_t prev_sat;
	int32_t sat_count;
	struct pg_kalman_state filter;
};

void pg_thermal_init(struct pg_thermal_state *state);

q16_t pg_thermal_update(struct pg_thermal_state *RESTRICT state, q16_t cpu_temp,
			q16_t bat_temp,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now);

#endif // PG_THERMAL_H
