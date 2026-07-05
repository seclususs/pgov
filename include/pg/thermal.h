// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_THERMAL_H
#define PG_THERMAL_H

#include "compiler.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define SMITH_BUFFER 512

struct pg_thermal_cfg {
	float limit_cpu;
	float limit_bat;
	float temp_cool;
	float temp_hot;
	float kp_base;
	float ki_base;
	float kd_base;
	float kp_fast;
	float ki_fast;
	float kd_fast;
	float aw_k;
	float filt_n;
	float ff_gain;
	float ff_lead_t;
	float ff_lag_t;
	float smith_gain;
	float smith_tau;
	float smith_delay;
};

struct ALIGNED(32) pg_hist_p {
	float value;
	struct timespec ts;
};

struct pg_smith_pred {
	struct pg_hist_p buf[SMITH_BUFFER];
	size_t head;
	size_t tail;
	size_t count;
	float ndelay;
};

struct pg_lead_lag_state {
	float prev_y;
	float prev_u;
	bool first_run;
};

struct pg_thermal_state {
	struct timespec last_tick;
	float integ;
	float prev_pv;
	float prev_d;
	float prev_sat;
	struct pg_lead_lag_state ff;
	struct pg_smith_pred smith;
};

void pg_thermal_init(struct pg_thermal_state *state);

float pg_thermal_update(struct pg_thermal_state *RESTRICT state, float cpu_temp,
			float bat_temp, float psi_load,
			const struct pg_thermal_cfg *RESTRICT cfg,
			const struct timespec *RESTRICT now);

#endif // PG_THERMAL_H
