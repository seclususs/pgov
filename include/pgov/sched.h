// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SCHED_H
#define PGOV_SCHED_H

#include "pgov/compiler.h"
#include "pgov/math.h"
#include <stdbool.h>

#define SCHED_UCLAMP_CEILING 1024

struct context;

struct sched_limit {
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

struct sched_param {
	q16_t lat_gran_rat;
	q16_t uclamp_k;
	q16_t sigmoid_k;
	q16_t surge_thresh;
	q16_t trans_rate;
	q16_t trans_diff;
	q16_t trans_poll;
};

struct sched_efficiency {
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

struct sched_demand {
	q16_t target_psi;
	q16_t velocity;
	q16_t dt_safe;
	q16_t therm_scale;
	q16_t trend_factor;
	q16_t integral;
	q16_t integral_dt;
	bool struct_break;
};

struct sched_state {
	q16_t psi_val;
	q16_t rate;
	q16_t psi_ref;
	q16_t prev_integral;
	bool first_run;
	struct sched_efficiency efficiency;
};

int sched_init(struct context *ctx);

/*
 * Snapshot active base values for crash-time recovery
 */
void sched_snapshot(const struct context *ctx);

/*
 * Async-signal-safe recovery hook restoring snapshot base values via pwrite
 */
void sched_restore(void);

/*
 * Restore tunables, release open descriptors, and cleanup sched node caches
 */
void sched_cleanup(struct context *ctx);

PURE bool sched_transient(const struct sched_state *state,
			  const struct sched_param *param);

/*
 * Update energy-battery tracking integral and derivative terms in Q16 format
 * for governor PID workload scaling
 */
void sched_update_integral(struct sched_state *RESTRICT state, q16_t bat_level,
			   q16_t dt_real, q16_t *RESTRICT integral,
			   q16_t *RESTRICT integral_dt);

void sched_update_efficiency(struct sched_efficiency *efficiency,
			     q16_t bat_level, q16_t therm_scale, q16_t avg300);

/*
 * Calculate required CPU load demand in Q16 format based on current
 * system pressure, velocity, thermal scaling, and integral history
 */
q16_t sched_load_demand(struct sched_state *state,
			const struct sched_demand *demand,
			const struct sched_param *param);

PURE q16_t sched_trend_gain(q16_t velocity);

PURE q16_t sched_effective_pressure(q16_t demand, q16_t trend_factor,
				    const struct sched_efficiency *efficiency);

PURE q16_t sched_thermal_latency(q16_t therm_scale,
				 const struct sched_limit *limit);

PURE q16_t sched_latency(q16_t pressure, q16_t demand, q16_t therm_lat,
			 const struct sched_param *param,
			 const struct sched_efficiency *efficiency,
			 const struct sched_limit *limit);

PURE q16_t sched_granularity(q16_t latency, const struct sched_param *param,
			     const struct sched_limit *limit);

PURE q16_t sched_wakeup(q16_t pressure,
			const struct sched_efficiency *efficiency,
			const struct sched_limit *limit);

PURE q16_t sched_migration(q16_t velocity, q16_t pressure,
			   const struct sched_limit *limit);

PURE q16_t sched_walt(q16_t pressure, const struct sched_limit *limit);

PURE q16_t sched_uclamp_min(q16_t pressure, q16_t therm_scale,
			    const struct sched_param *param,
			    const struct sched_efficiency *efficiency,
			    const struct sched_limit *limit);

PURE q16_t sched_uclamp_max(q16_t therm_scale, q16_t bat_level,
			    const struct sched_limit *limit);

#endif // PGOV_SCHED_H
