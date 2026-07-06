// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_THERMAL_H
#define PG_THERMAL_H

#include "compiler.h"
#include "pg/math.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define SMITH_BUFFER 512

struct pg_thermal_cfg {
	q16_t limit_cpu;
	q16_t limit_bat;
	q16_t temp_cool;
	q16_t temp_hot;
	q16_t kp_base;
	q16_t ki_base;
	q16_t kd_base;
	q16_t kp_fast;
	q16_t ki_fast;
	q16_t kd_fast;
	q16_t aw_k;
	q16_t filt_n;
	q16_t ff_gain;
	q16_t ff_lead_t;
	q16_t ff_lag_t;
	q16_t smith_gain;
	q16_t smith_tau;
	q16_t smith_delay;
};

struct ALIGNED(32) pg_hist_p {
	q16_t value;
	struct timespec ts;
};

struct pg_smith_pred {
	struct pg_hist_p buf[SMITH_BUFFER];
	size_t head;
	size_t tail;
	size_t count;
	q16_t ndelay;
};

struct pg_lead_lag_state {
	q16_t prev_y;
	q16_t prev_u;
	bool first_run;
};

struct pg_thermal_state {
	struct timespec last_tick;
	q16_t integ;
	q16_t prev_pv;
	q16_t prev_d;
	q16_t prev_sat;
	struct pg_lead_lag_state ff;
	struct pg_smith_pred smith;
};

void pg_thermal_init(struct pg_thermal_state *state);

q16_t pg_thermal_update(struct pg_thermal_state *RESTRICT state, q16_t cpu_temp,
			q16_t bat_temp, q16_t psi_load,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now);

#endif // PG_THERMAL_H
