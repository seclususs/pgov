// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_CPU_H
#define PG_CPU_H

#include "compiler.h"
#include "pg/math.h"
#include <stdbool.h>

struct pg_cpu_lim {
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
};

struct pg_cpu_cfg {
	q16_t lat_gran_rat;
	q16_t uclamp_k;
	q16_t stab_rat;
	q16_t gain_alpha;
	q16_t sigmoid_k;
	q16_t surge_thresh;
	q16_t trans_rate;
	q16_t trans_diff;
	q16_t trans_poll;
	q16_t nis_thresh;
};

struct pg_cpu_eff {
	q16_t resp_gain;
	q16_t surge_gain;
	q16_t trend_amp;
	q16_t lookahead;
	q16_t decay;
	q16_t sig_mid;
	q16_t ucl_mid;
};

struct pg_demand_input {
	q16_t tgt_psi;
	q16_t vel;
	q16_t dt_real;
	q16_t dt_safe;
	q16_t therm_scale;
	q16_t trend_fact;
	q16_t integ;
	q16_t integ_dt;
	bool struct_break;
};

struct pg_load_state {
	q16_t psi_val;
	q16_t rate;
	q16_t prev_integ;
	bool first_run;
	struct pg_cpu_eff eff;
};

PURE bool pg_cpu_trans(const struct pg_load_state *RESTRICT state,
		       q16_t target_psi, const struct pg_cpu_cfg *RESTRICT cfg);

void pg_cpu_upd_intg(struct pg_load_state *RESTRICT state, q16_t bat_lvl,
		     q16_t dt_real, q16_t *RESTRICT i, q16_t *RESTRICT i_dt);

void pg_cpu_upd_eff(struct pg_cpu_eff *RESTRICT eff, q16_t bat_lvl,
		    q16_t th_scl, q16_t avg300);

q16_t pg_cpu_calc_load_demand(struct pg_load_state *RESTRICT state,
			      const struct pg_demand_input *RESTRICT input,
			      const struct pg_cpu_cfg *RESTRICT cfg);

PURE q16_t pg_cpu_calc_trend_gain(q16_t vel);

PURE q16_t pg_cpu_calc_eff_press(q16_t l_dem, q16_t t_fact,
				 const struct pg_cpu_eff *RESTRICT eff);

PURE q16_t pg_cpu_calc_therm_lat(q16_t th_scl, const struct pg_cpu_lim *lim);

PURE q16_t pg_cpu_calc_lat(q16_t p_eff, q16_t l_dem, q16_t th_lat,
			   const struct pg_cpu_cfg *RESTRICT cfg,
			   const struct pg_cpu_eff *RESTRICT eff,
			   const struct pg_cpu_lim *RESTRICT lim);

PURE q16_t pg_cpu_calc_gran(q16_t lat, const struct pg_cpu_cfg *RESTRICT cfg,
			    const struct pg_cpu_lim *RESTRICT lim);

PURE q16_t pg_cpu_calc_wakeup(q16_t p_eff,
			      const struct pg_cpu_eff *RESTRICT eff,
			      const struct pg_cpu_lim *RESTRICT lim);

PURE q16_t pg_cpu_calc_migration(q16_t vel, q16_t p_eff,
				 const struct pg_cpu_lim *lim);

PURE q16_t pg_cpu_calc_walt(q16_t press, const struct pg_cpu_lim *lim);

PURE q16_t pg_cpu_calc_uclamp(q16_t press, q16_t th_scl,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_eff *RESTRICT eff,
			      const struct pg_cpu_lim *RESTRICT lim);

#endif // PG_CPU_H
