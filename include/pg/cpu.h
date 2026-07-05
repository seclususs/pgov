// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_CPU_H
#define PG_CPU_H

#include "compiler.h"
#include <stdbool.h>
#include <stdint.h>

struct pg_cpu_lim {
	float min_lat;
	float max_lat;
	float min_gran;
	float max_gran;
	float min_wake;
	float max_wake;
	float min_mig;
	float max_mig;
	float min_walt;
	float max_walt;
	float min_uclamp;
	float max_uclamp;
};

struct pg_cpu_cfg {
	float lat_gran_rat;
	float decay;
	float uclamp_k;
	float uclamp_mid;
	float resp_gain;
	float stab_rat;
	float stab_marg;
	float gain_alpha;
	float sigmoid_k;
	float sigmoid_mid;
	float lookahead;
	float trend_amp;
	float surge_thresh;
	float surge_gain;
	float trans_rate;
	float trans_diff;
	float trans_poll;
	float nis_thresh;
	float bat_wt;
};

struct pg_ctrl_cfg {
	float press_wt;
	float deriv_wt;
	uint64_t bat_chk_sec;
	int32_t thresh_us;
	int32_t win_us;
};

struct pg_demand_input {
	float tgt_psi;
	float vel;
	float dt_real;
	float dt_safe;
	float therm_scale;
	float trend_fact;
	float integ;
	float integ_dt;
	bool struct_break;
};

struct pg_load_state {
	float psi_val;
	float rate;
	float prev_integ;
	bool first_run;
};

PURE bool pg_cpu_trans(const struct pg_load_state *RESTRICT state,
		       float target_psi, const struct pg_cpu_cfg *RESTRICT cfg);

void pg_cpu_upd_integ_params(struct pg_load_state *RESTRICT state,
			     float bat_level, float dt_safe,
			     const struct pg_cpu_cfg *RESTRICT cfg,
			     float *RESTRICT integ, float *RESTRICT integ_dt);

float pg_cpu_calc_load_demand(struct pg_load_state *RESTRICT state,
			      const struct pg_demand_input *RESTRICT input,
			      const struct pg_cpu_cfg *RESTRICT cfg);

PURE float pg_cpu_calc_trend_gain(float velocity);

PURE float pg_cpu_calc_eff_press(float load_demand, float trend_fact,
				 const struct pg_cpu_cfg *cfg);

PURE float pg_cpu_calc_therm_lat(float therm_scale,
				 const struct pg_cpu_lim *lim);

void pg_cpu_calc_lat_gran(float p_eff, float load_demand, float therm_lat,
			  const struct pg_cpu_cfg *RESTRICT cfg,
			  const struct pg_cpu_lim *RESTRICT lim,
			  float *RESTRICT lat, float *RESTRICT gran);

PURE float pg_cpu_calc_wakeup(float p_eff,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim);

PURE float pg_cpu_calc_migration(float velocity, float p_eff,
				 const struct pg_cpu_lim *lim);

PURE float pg_cpu_calc_walt(float pressure, const struct pg_cpu_lim *lim);

PURE float pg_cpu_calc_uclamp(float pressure, float therm_scale,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim);

#endif // PG_CPU_H
