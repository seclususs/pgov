// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_CPU_H
#define PGOV_CPU_H

#include "pgov/compiler.h"
#include "pgov/math.h"
#include <stdbool.h>

#define CPU_UCLAMP_MAX_CEILING 1024

struct cpu_limit {
	q16_t min_lat;
	q16_t max_lat;
	q16_t min_gran;
	q16_t max_gran;
	q16_t min_wake;
	q16_t max_wake;
	q16_t min_mig;
	q16_t max_mig;
	q16_t min_walt;
	q16_t max_walt;
	q16_t min_ucl;
	q16_t max_ucl;
	q16_t ucl_max;
	q16_t susp_ucl;
};

struct cpu_param {
	q16_t lat_gran_rat;
	q16_t uclamp_k;
	q16_t sigmoid_k;
	q16_t surge_thresh;
	q16_t trans_rate;
	q16_t trans_diff;
	q16_t trans_poll;
};

struct cpu_efficiency {
	q16_t resp_gain;
	q16_t surge_gain;
	q16_t trend_amp;
	q16_t lookahead;
	q16_t decay;
	q16_t sigmoid_mid;
	q16_t uclamp_mid;
	q16_t stab_ratio;
	q16_t gain_alpha;
	q16_t nis_thresh;
};

struct cpu_demand {
	q16_t target_psi;
	q16_t velocity;
	q16_t dt_safe;
	q16_t therm_scale;
	q16_t trend_factor;
	q16_t integral;
	q16_t integral_dt;
	bool struct_break;
};

struct cpu_state {
	q16_t psi_val;
	q16_t rate;
	q16_t psi_ref;
	q16_t prev_integral;
	bool first_run;
	struct cpu_efficiency efficiency;
};

PURE bool cpu_transient(const struct cpu_state *state,
			const struct cpu_param *param);

void cpu_update_integral(struct cpu_state *RESTRICT state, q16_t bat_level,
			 q16_t dt_real, q16_t *RESTRICT integral,
			 q16_t *RESTRICT integral_dt);

void cpu_update_efficiency(struct cpu_efficiency *efficiency, q16_t bat_level,
			   q16_t therm_scale, q16_t avg300);

q16_t cpu_calc_load_demand(struct cpu_state *state,
			   const struct cpu_demand *demand,
			   const struct cpu_param *param);

PURE q16_t cpu_calc_trend_gain(q16_t velocity);

PURE q16_t cpu_calc_effective_pressure(q16_t demand, q16_t trend_factor,
				       const struct cpu_efficiency *efficiency);

PURE q16_t cpu_calc_thermal_latency(q16_t therm_scale,
				    const struct cpu_limit *limit);

PURE q16_t cpu_calc_latency(q16_t pressure, q16_t demand, q16_t therm_lat,
			    const struct cpu_param *param,
			    const struct cpu_efficiency *efficiency,
			    const struct cpu_limit *limit);

PURE q16_t cpu_calc_granularity(q16_t latency, const struct cpu_param *param,
				const struct cpu_limit *limit);

PURE q16_t cpu_calc_wakeup(q16_t pressure,
			   const struct cpu_efficiency *efficiency,
			   const struct cpu_limit *limit);

PURE q16_t cpu_calc_migration(q16_t velocity, q16_t pressure,
			      const struct cpu_limit *limit);

PURE q16_t cpu_calc_walt(q16_t pressure, const struct cpu_limit *limit);

PURE q16_t cpu_calc_uclamp_min(q16_t pressure, q16_t therm_scale,
			       const struct cpu_param *param,
			       const struct cpu_efficiency *efficiency,
			       const struct cpu_limit *limit);

PURE q16_t cpu_calc_uclamp_max(q16_t therm_scale, q16_t bat_level,
			       const struct cpu_limit *limit);

#endif // PGOV_CPU_H
