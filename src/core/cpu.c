// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/cpu.h"

PURE bool pg_cpu_trans(const struct pg_load_state *RESTRICT state,
		       q16_t target_psi, const struct pg_cpu_cfg *RESTRICT cfg)
{
	bool rate = ABS_Q16(state->rate) > cfg->trans_rate;
	bool diff = ABS_Q16(state->psi_val - target_psi) > cfg->trans_diff;
	return (rate || diff) != 0;
}

void pg_cpu_upd_integ_params(struct pg_load_state *RESTRICT state,
			     q16_t bat_level, q16_t dt_safe,
			     const struct pg_cpu_cfg *RESTRICT cfg,
			     q16_t *RESTRICT integ, q16_t *RESTRICT integ_dt)
{
	q16_t diff_bat = INT_TO_Q16(100) - bat_level;
	if (diff_bat < 0)
		diff_bat = 0;

	q16_t depl = q16_div(diff_bat, INT_TO_Q16(100));
	q16_t depl2 = q16_mul(depl, depl);
	q16_t depl3 = q16_mul(depl2, depl);
	q16_t cur_integ = q16_mul(cfg->bat_wt, depl3);

	if (UNLIKELY(state->first_run)) {
		state->prev_integ = cur_integ;
		state->first_run = false;
		*integ = cur_integ;
		*integ_dt = 0;
		return;
	}

	q16_t cur_dt = 0;
	if (LIKELY(dt_safe > 0)) {
		q16_t d_integ = cur_integ - state->prev_integ;
		cur_dt = q16_div(d_integ, dt_safe);
	}

	state->prev_integ = cur_integ;

	*integ = cur_integ;
	*integ_dt = cur_dt;
}

q16_t pg_cpu_calc_load_demand(struct pg_load_state *RESTRICT state,
			      const struct pg_demand_input *RESTRICT input,
			      const struct pg_cpu_cfg *RESTRICT cfg)
{
	if (UNLIKELY(input->struct_break)) {
		state->psi_val = input->tgt_psi;
		state->rate = 0;
	}

	q16_t load_rate = input->vel;
	if (ABS_Q16(load_rate) > cfg->surge_thresh)
		state->rate += q16_mul(load_rate, cfg->surge_gain);

	q16_t pred = input->tgt_psi + q16_mul(load_rate, cfg->lookahead);
	q16_t trend = q16_mul(cfg->gain_alpha, input->trend_fact);
	q16_t k_dyn = q16_mul(cfg->resp_gain, Q16_ONE + trend);

	q16_t therm =
		pg_math_clamp(input->therm_scale, FLOAT_TO_Q16(0.1F), Q16_ONE);

	q16_t k_final = q16_mul(k_dyn, q16_mul(therm, therm));
	q16_t disp = pred - state->psi_val;

	q32_t p_term = q32_mul(Q16_TO_Q32(k_final), Q16_TO_Q32(disp));

	q32_t l_term =
		q32_mul(Q16_TO_Q32(input->integ), Q16_TO_Q32(state->psi_val));

	q32_t max_resp =
		q32_mul(Q16_TO_Q32(k_final), Q16_TO_Q32(INT_TO_Q16(100)));

	q32_t limit_l = q32_mul(max_resp, Q16_TO_Q32(FLOAT_TO_Q16(1.5F)));

	if (l_term > limit_l)
		l_term = limit_l;

	q16_t c_crit = q16_mul(INT_TO_Q16(2), pg_math_q16_sqrt(k_final));
	q16_t c_base = q16_mul(c_crit, cfg->stab_rat);

	q32_t load = Q16_TO_Q32(load_rate);
	q32_t rate_sq = q32_mul(load, load) + Q16_TO_Q32(FLOAT_TO_Q16(0.001F));

	q32_t d_num1 = Q16_TO_Q32(q16_mul(Q16_HALF, ABS_Q16(input->integ_dt)));
	q32_t psi_val = Q16_TO_Q32(state->psi_val);
	q32_t d_num2 = q32_mul(psi_val, psi_val);
	q32_t d_num = q32_mul(d_num1, d_num2);

	q16_t c_req = 0;
	if (rate_sq > 0)
		c_req = (q16_t)(d_num / (rate_sq >> Q16_SHIFT));

	q16_t c_max = q16_mul(c_base, INT_TO_Q16(4));
	q16_t c_clamp = pg_math_clamp(c_req, 0, c_max);
	q16_t c_stab = q16_mul(c_clamp, cfg->stab_marg);

	q16_t therm_sqrt = pg_math_q16_sqrt(therm);
	q16_t c_therm = (therm_sqrt > 0) ? q16_div(c_base, therm_sqrt) : c_base;
	q16_t c_final = (c_therm > c_stab) ? c_therm : c_stab;

	q32_t d_term = q32_mul(Q16_TO_Q32(c_final), Q16_TO_Q32(state->rate));
	q32_t net_q32 = p_term - d_term - l_term;
	if (net_q32 > Q16_TO_Q32(INT_TO_Q16(32000)))
		net_q32 = Q16_TO_Q32(INT_TO_Q16(32000));
	else if (net_q32 < Q16_TO_Q32(INT_TO_Q16(-32000)))
		net_q32 = Q16_TO_Q32(INT_TO_Q16(-32000));

	q16_t net = Q32_TO_Q16(net_q32);
	state->rate += q16_mul(net, input->dt_safe);
	state->psi_val += q16_mul(state->rate, input->dt_real);

	if (UNLIKELY(state->psi_val < 0)) {
		state->psi_val = 0;
		state->rate = 0;
	} else if (UNLIKELY(state->psi_val > INT_TO_Q16(500))) {
		state->psi_val = INT_TO_Q16(500);
		state->rate = 0;
	}

	return state->psi_val;
}

PURE q16_t pg_cpu_calc_trend_gain(q16_t velocity)
{
	if (velocity > 0)
		return pg_math_alg_sig(velocity);

	return 0;
}

PURE q16_t pg_cpu_calc_eff_press(q16_t l_dem, q16_t trend_fact,
				 const struct pg_cpu_cfg *cfg)
{
	q16_t trend_mul = Q16_ONE + q16_mul(trend_fact, cfg->trend_amp);
	return q16_mul(l_dem, trend_mul);
}

PURE q16_t pg_cpu_calc_therm_lat(q16_t therm_scale,
				 const struct pg_cpu_lim *lim)
{
	q16_t lim_ratio = pg_math_clamp(Q16_ONE - therm_scale, 0, Q16_ONE);
	q16_t lat_range = lim->max_lat - lim->min_lat;
	return lim->min_lat + q16_mul(lat_range, lim_ratio);
}

void pg_cpu_calc_lat_gran(q16_t p_eff, q16_t l_dem, q16_t therm_lat,
			  const struct pg_cpu_cfg *RESTRICT cfg,
			  const struct pg_cpu_lim *RESTRICT lim,
			  q16_t *RESTRICT lat, q16_t *RESTRICT gran)
{
	q16_t sig = pg_math_sigmoid(p_eff, cfg->sigmoid_k, cfg->sigmoid_mid);
	q16_t factor = Q16_ONE - sig;
	q16_t range = lim->max_lat - lim->min_lat;
	q16_t norm_lat = lim->min_lat + q16_mul(range, factor);
	q16_t eff_dem =
		pg_math_clamp(q16_div(l_dem, INT_TO_Q16(100)), 0, Q16_ONE);

	q16_t low_lat = lim->max_lat - q16_mul(eff_dem, range);
	q16_t ideal_lat = (norm_lat < low_lat) ? norm_lat : low_lat;
	q16_t final_lat = (ideal_lat > therm_lat) ? ideal_lat : therm_lat;
	q16_t adj_lat = pg_math_clamp(final_lat, lim->min_lat, lim->max_lat);

	q16_t raw_gran = q16_mul(adj_lat, cfg->lat_gran_rat);
	q16_t adj_gran = pg_math_clamp(raw_gran, lim->min_gran, lim->max_gran);
	q16_t final_gran = (adj_gran < adj_lat) ? adj_gran : adj_lat;

	*lat = adj_lat;
	*gran = final_gran;
}

PURE q16_t pg_cpu_calc_wakeup(q16_t p_eff,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t decay = pg_math_decay(p_eff, cfg->decay);
	q16_t range = lim->max_wake - lim->min_wake;
	q16_t raw_wake = lim->min_wake + q16_mul(range, decay);
	return pg_math_clamp(raw_wake, lim->min_wake, lim->max_wake);
}

PURE q16_t pg_cpu_calc_migration(q16_t velocity, q16_t p_eff,
				 const struct pg_cpu_lim *lim)
{
	q16_t x = pg_math_clamp(q16_div(p_eff, INT_TO_Q16(100)), 0, Q16_ONE);
	q16_t factor = Q16_ONE - x;
	q16_t curve = q16_mul(factor, factor);
	q16_t range = lim->max_mig - lim->min_mig;
	q16_t raw_mig = lim->min_mig + q16_mul(range, curve);
	q16_t abs_vel = ABS_Q16(velocity);
	q16_t vol_rat =
		pg_math_clamp(q16_div(abs_vel, INT_TO_Q16(25)), 0, Q16_ONE);

	q16_t mig = q16_mul(raw_mig, Q16_ONE - q16_mul(vol_rat, Q16_HALF));
	return pg_math_clamp(mig, lim->min_mig, lim->max_mig);
}

PURE q16_t pg_cpu_calc_walt(q16_t pressure, const struct pg_cpu_lim *lim)
{
	q16_t ratio = q16_div(pressure, INT_TO_Q16(100));
	q16_t curve = q16_mul(ratio, ratio);
	q16_t range = lim->max_walt - lim->min_walt;
	q16_t val = lim->min_walt + q16_mul(range, curve);
	return pg_math_clamp(val, lim->min_walt, lim->max_walt);
}

PURE q16_t pg_cpu_calc_uclamp(q16_t pressure, q16_t therm_scale,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t sig = pg_math_sigmoid(pressure, cfg->uclamp_k, cfg->uclamp_mid);
	q16_t range = lim->max_uclamp - lim->min_uclamp;
	q16_t ideal_uclamp = lim->min_uclamp + q16_mul(range, sig);
	return pg_math_clamp(q16_mul(ideal_uclamp, therm_scale),
			     lim->min_uclamp, lim->max_uclamp);
}
