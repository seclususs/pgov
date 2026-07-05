// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/cpu.h"
#include "pg/math.h"
#include <math.h>

PURE bool pg_cpu_trans(const struct pg_load_state *RESTRICT state,
		       float target_psi, const struct pg_cpu_cfg *RESTRICT cfg)
{
	bool rate = fabsf(state->rate) > cfg->trans_rate;
	bool diff = fabsf(state->psi_val - target_psi) > cfg->trans_diff;
	return (rate || diff) != 0;
}

void pg_cpu_upd_integ_params(struct pg_load_state *RESTRICT state,
			     float bat_level, float dt_safe,
			     const struct pg_cpu_cfg *RESTRICT cfg,
			     float *RESTRICT integ, float *RESTRICT integ_dt)
{
	float depl = fmaxf(100.0F - bat_level, 0.0F) / 100.0F;
	float depl3 = depl * depl * depl;
	float cur_integ = cfg->bat_wt * depl3;

	if (UNLIKELY(state->first_run)) {
		state->prev_integ = cur_integ;
		state->first_run = false;
		*integ = cur_integ;
		*integ_dt = 0.0F;
		return;
	}

	float cur_dt = 0.0F;
	if (LIKELY(dt_safe > 0.0F)) {
		float d_integ = cur_integ - state->prev_integ;
		cur_dt = d_integ / dt_safe;
	}

	state->prev_integ = cur_integ;

	*integ = cur_integ;
	*integ_dt = cur_dt;
}

float pg_cpu_calc_load_demand(struct pg_load_state *RESTRICT state,
			      const struct pg_demand_input *RESTRICT input,
			      const struct pg_cpu_cfg *RESTRICT cfg)
{
	if (UNLIKELY(input->struct_break)) {
		state->psi_val = input->tgt_psi;
		state->rate = 0.0F;
	}

	float load_rate = input->vel;
	if (fabsf(load_rate) > cfg->surge_thresh)
		state->rate += load_rate * cfg->surge_gain;

	float pred = input->tgt_psi + (load_rate * cfg->lookahead);

	float trend = cfg->gain_alpha * input->trend_fact;
	float k_dyn = cfg->resp_gain * (1.0F + trend);
	float therm = pg_math_clampf(input->therm_scale, 0.1F, 1.0F);
	float k_final = k_dyn * (therm * therm);

	float disp = pred - state->psi_val;
	float p_term = k_final * disp;

	float l_term = input->integ * state->psi_val;
	float max_resp = k_final * 100.0F;
	l_term = fminf(l_term, max_resp * 1.5F);

	float c_crit = 2.0F * sqrtf(k_final);
	float c_base = c_crit * cfg->stab_rat;

	float rate_sq = (load_rate * load_rate) + 0.001F;

	float d_num = 0.5F * fabsf(input->integ_dt) *
		      (state->psi_val * state->psi_val);

	float c_req = d_num / rate_sq;

	float c_max = c_base * 4.0F;
	float c_clamp = pg_math_clampf(c_req, 0.0F, c_max);
	float c_stab = c_clamp * cfg->stab_marg;

	float c_therm = c_base / sqrtf(therm);
	float c_final = fmaxf(c_therm, c_stab);
	float d_term = c_final * state->rate;
	float net = p_term - d_term - l_term;
	float dv = net;

	state->rate += dv * input->dt_safe;
	state->psi_val += state->rate * input->dt_real;

	if (UNLIKELY(state->psi_val < 0.0F)) {
		state->psi_val = 0.0F;
		state->rate = 0.0F;
	} else if (UNLIKELY(state->psi_val > 500.0F)) {
		state->psi_val = 500.0F;
		state->rate = 0.0F;
	}

	return state->psi_val;
}

PURE float pg_cpu_calc_trend_gain(float velocity)
{
	if (velocity > 0.0F)
		return pg_math_tanh_approx(velocity);

	return 0.0F;
}

PURE float pg_cpu_calc_eff_press(float load_demand, float trend_fact,
				 const struct pg_cpu_cfg *cfg)
{
	float trend_mul = 1.0F + (trend_fact * cfg->trend_amp);
	return load_demand * trend_mul;
}

PURE float pg_cpu_calc_therm_lat(float therm_scale,
				 const struct pg_cpu_lim *lim)
{
	float lim_ratio = pg_math_clampf(1.0F - therm_scale, 0.0F, 1.0F);
	float lat_range = lim->max_lat - lim->min_lat;
	return lim->min_lat + (lat_range * lim_ratio);
}

void pg_cpu_calc_lat_gran(float p_eff, float load_demand, float therm_lat,
			  const struct pg_cpu_cfg *RESTRICT cfg,
			  const struct pg_cpu_lim *RESTRICT lim,
			  float *RESTRICT lat, float *RESTRICT gran)
{
	float sigmoid_val =
		pg_math_sigmoid(p_eff, cfg->sigmoid_k, cfg->sigmoid_mid);

	float factor = 1.0F - sigmoid_val;
	float range = lim->max_lat - lim->min_lat;
	float norm_lat = lim->min_lat + (range * factor);
	float eff_dem = pg_math_clampf(load_demand / 100.0F, 0.0F, 1.0F);

	float low_lat = lim->max_lat - (eff_dem * range);
	float ideal_lat = fminf(norm_lat, low_lat);
	float final_lat = fmaxf(ideal_lat, therm_lat);
	float adj_lat = pg_math_clampf(final_lat, lim->min_lat, lim->max_lat);

	float raw_gran = adj_lat * cfg->lat_gran_rat;
	float clamp_gran =
		pg_math_clampf(raw_gran, lim->min_gran, lim->max_gran);

	float final_gran = fminf(clamp_gran, adj_lat);

	*lat = adj_lat;
	*gran = final_gran;
}

PURE float pg_cpu_calc_wakeup(float p_eff,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	float decay = pg_math_decay(p_eff, cfg->decay);
	float range = lim->max_wake - lim->min_wake;
	float raw_wake = lim->min_wake + (range * decay);
	return pg_math_clampf(raw_wake, lim->min_wake, lim->max_wake);
}

PURE float pg_cpu_calc_migration(float velocity, float p_eff,
				 const struct pg_cpu_lim *lim)
{
	float x = pg_math_clampf(p_eff / 100.0F, 0.0F, 1.0F);
	float factor = 1.0F - x;
	float curve = factor * factor;
	float range = lim->max_mig - lim->min_mig;
	float raw_mig = lim->min_mig + (range * curve);
	float vol_rat = pg_math_clampf(fabsf(velocity) / 25.0F, 0.0F, 1.0F);
	float mig = raw_mig * (1.0F - (vol_rat * 0.5F));
	return pg_math_clampf(mig, lim->min_mig, lim->max_mig);
}

PURE float pg_cpu_calc_walt(float pressure, const struct pg_cpu_lim *lim)
{
	float ratio = pressure / 100.0F;
	float curve = ratio * ratio;
	float range = lim->max_walt - lim->min_walt;
	float val = lim->min_walt + (range * curve);
	return pg_math_clampf(val, lim->min_walt, lim->max_walt);
}

PURE float pg_cpu_calc_uclamp(float pressure, float therm_scale,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	float sigmoid_val =
		pg_math_sigmoid(pressure, cfg->uclamp_k, cfg->uclamp_mid);

	float range = lim->max_uclamp - lim->min_uclamp;
	float idl = lim->min_uclamp + (range * sigmoid_val);
	return pg_math_clampf(idl * therm_scale, lim->min_uclamp,
			      lim->max_uclamp);
}
