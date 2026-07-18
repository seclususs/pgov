// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/cpu.h"

PURE bool pg_cpu_trans(const struct pg_load_state *RESTRICT state,
		       q16_t target_psi, const struct pg_cpu_cfg *RESTRICT cfg)
{
	bool rate = abs_q16(state->rate) > cfg->trans_rate;
	bool diff = abs_q16(state->psi_val - target_psi) > cfg->trans_diff;
	return (rate || diff) != 0;
}

void pg_cpu_upd_intg(struct pg_load_state *RESTRICT state, q16_t bat_lvl,
		     q16_t dt_real, q16_t *RESTRICT i, q16_t *RESTRICT i_dt)
{
	q16_t d_bat = INT_TO_Q16(100) - bat_lvl;
	if (d_bat < 0)
		d_bat = 0;

	q16_t depl = q16_div(d_bat, INT_TO_Q16(100));
	q16_t depl2 = q16_mul(depl, depl);
	q16_t depl3 = q16_mul(depl2, depl);
	q16_t c_intg = q16_mul(FLOAT_TO_Q16(97.0F), depl3);

	if (UNLIKELY(state->first_run)) {
		state->prev_integ = c_intg;
		state->first_run = false;
		*i = c_intg;
		*i_dt = 0;
		return;
	}

	q16_t c_dt = 0;
	if (LIKELY(dt_real > 0)) {
		q16_t diff = c_intg - state->prev_integ;
		q16_t tau = INT_TO_Q16(5);

		q16_t alpha = q16_div(dt_real, tau);
		if (alpha > Q16_ONE)
			alpha = Q16_ONE;

		q16_t step = q16_mul(diff, alpha);
		state->prev_integ += step;
		c_dt = q16_div(diff, tau);
	}

	state->prev_integ = c_intg;
	*i = c_intg;
	*i_dt = c_dt;
}

static inline q32_t calc_l_term(q16_t integ, q16_t psi_val, q16_t k_fin)
{
	q32_t l_term = q32_mul(Q16_TO_Q32(integ), Q16_TO_Q32(psi_val));
	q32_t m_rsp = q32_mul(Q16_TO_Q32(k_fin), Q16_TO_Q32(INT_TO_Q16(100)));
	q32_t lim_l = q32_mul(m_rsp, Q16_TO_Q32(FLOAT_TO_Q16(1.5F)));
	return (l_term > lim_l) ? lim_l : l_term;
}

static inline q16_t calc_c_final(q16_t k_fin, q16_t vel, q16_t i_dt,
				 q16_t psi_val, q16_t th,
				 const struct pg_cpu_cfg *cfg)
{
	q16_t c_crit = q16_mul(INT_TO_Q16(2), pg_math_q16_sqrt(k_fin));
	q16_t c_base = q16_mul(c_crit, cfg->stab_rat);
	q32_t load = Q16_TO_Q32(vel);
	q32_t r_sq = q32_mul(load, load) + Q16_TO_Q32(FLOAT_TO_Q16(0.001F));

	q32_t d_n1 = Q16_TO_Q32(q16_mul(Q16_HALF, abs_q16(i_dt)));
	q32_t p_val = Q16_TO_Q32(psi_val);
	q32_t d_n2 = q32_mul(p_val, p_val);
	q32_t d_num = q32_mul(d_n1, d_n2);

	q16_t c_req = (r_sq > 0) ? (q16_t)(d_num / (r_sq >> Q16_SHIFT)) : 0;
	q16_t c_max = q16_mul(c_base, INT_TO_Q16(4));
	q16_t c_clp = pg_math_clamp(c_req, 0, c_max);
	q16_t c_stb = q16_mul(c_clp, FLOAT_TO_Q16(3.1F));

	q16_t th_sq = pg_math_q16_sqrt(th);
	q16_t c_th = (th_sq > 0) ? q16_div(c_base, th_sq) : c_base;

	return (c_th > c_stb) ? c_th : c_stb;
}

void pg_cpu_upd_eff(struct pg_cpu_eff *RESTRICT eff, q16_t bat_lvl,
		    q16_t th_scl, q16_t avg300)
{
	q16_t bat_fact = q16_div(bat_lvl, INT_TO_Q16(100));
	q16_t health = q16_mul(bat_fact, th_scl);

	eff->resp_gain = q16_mul(FLOAT_TO_Q16(31.0F), health);
	eff->surge_gain = q16_mul(FLOAT_TO_Q16(0.095F), health);
	eff->trend_amp = q16_mul(FLOAT_TO_Q16(0.115F), health);
	eff->lookahead = q16_mul(FLOAT_TO_Q16(0.17F), th_scl);
	eff->decay = q16_mul(FLOAT_TO_Q16(0.019F), th_scl);
	eff->sig_mid = FLOAT_TO_Q16(6.8F) + q16_mul(FLOAT_TO_Q16(0.5F), avg300);
	eff->ucl_mid = eff->sig_mid + FLOAT_TO_Q16(2.7F);
}

q16_t pg_cpu_calc_load_demand(struct pg_load_state *RESTRICT state,
			      const struct pg_demand_input *RESTRICT input,
			      const struct pg_cpu_cfg *RESTRICT cfg)
{
	struct pg_cpu_eff *eff = &state->eff;

	if (UNLIKELY(input->struct_break)) {
		state->psi_val = input->tgt_psi;
		state->rate = 0;
	}

	q16_t l_rate = input->vel;
	if (abs_q16(l_rate) > cfg->surge_thresh)
		state->rate += q16_mul(l_rate, eff->surge_gain);

	q16_t pred = input->tgt_psi + q16_mul(l_rate, eff->lookahead);
	q16_t trend = q16_mul(cfg->gain_alpha, input->trend_fact);
	q16_t k_dyn = q16_mul(eff->resp_gain, Q16_ONE + trend);
	q16_t th =
		pg_math_clamp(input->therm_scale, FLOAT_TO_Q16(0.1F), Q16_ONE);

	q16_t k_fin = q16_mul(k_dyn, q16_mul(th, th));
	q16_t disp = pred - state->psi_val;
	q32_t p_term = q32_mul(Q16_TO_Q32(k_fin), Q16_TO_Q32(disp));

	q32_t l_term = calc_l_term(input->integ, state->psi_val, k_fin);
	q16_t c_fin = calc_c_final(k_fin, l_rate, input->integ_dt,
				   state->psi_val, th, cfg);

	q32_t d_term = q32_mul(Q16_TO_Q32(c_fin), Q16_TO_Q32(state->rate));
	q32_t n_q32 = p_term - d_term - l_term;

	if (n_q32 > Q16_TO_Q32(INT_TO_Q16(32000)))
		n_q32 = Q16_TO_Q32(INT_TO_Q16(32000));
	else if (n_q32 < Q16_TO_Q32(INT_TO_Q16(-32000)))
		n_q32 = Q16_TO_Q32(INT_TO_Q16(-32000));

	q16_t net = Q32_TO_Q16(n_q32);
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

PURE q16_t pg_cpu_calc_trend_gain(q16_t vel)
{
	if (vel > 0)
		return pg_math_alg_sig(vel);

	return 0;
}

PURE q16_t pg_cpu_calc_eff_press(q16_t l_dem, q16_t t_fact,
				 const struct pg_cpu_eff *eff)
{
	q16_t t_mul = Q16_ONE + q16_mul(t_fact, eff->trend_amp);
	return q16_mul(l_dem, t_mul);
}

PURE q16_t pg_cpu_calc_therm_lat(q16_t th_scl, const struct pg_cpu_lim *lim)
{
	q16_t l_rat = pg_math_clamp(Q16_ONE - th_scl, 0, Q16_ONE);
	q16_t l_rng = lim->max_lat - lim->min_lat;
	return lim->min_lat + q16_mul(l_rng, l_rat);
}

PURE q16_t pg_cpu_calc_lat(q16_t p_eff, q16_t l_dem, q16_t th_lat,
			   const struct pg_cpu_cfg *RESTRICT cfg,
			   const struct pg_cpu_eff *RESTRICT eff,
			   const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t sig = pg_math_sigmoid(p_eff, cfg->sigmoid_k, eff->sig_mid);
	q16_t fac = Q16_ONE - sig;
	q16_t rng = lim->max_lat - lim->min_lat;
	q16_t n_lat = lim->min_lat + q16_mul(rng, fac);
	q16_t e_dem =
		pg_math_clamp(q16_div(l_dem, INT_TO_Q16(100)), 0, Q16_ONE);

	q16_t l_lat = lim->max_lat - q16_mul(e_dem, rng);
	q16_t i_lat = (n_lat < l_lat) ? n_lat : l_lat;
	q16_t f_lat = (i_lat > th_lat) ? i_lat : th_lat;

	return pg_math_clamp(f_lat, lim->min_lat, lim->max_lat);
}

PURE q16_t pg_cpu_calc_gran(q16_t lat, const struct pg_cpu_cfg *RESTRICT cfg,
			    const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t r_grn = q16_mul(lat, cfg->lat_gran_rat);
	q16_t a_grn = pg_math_clamp(r_grn, lim->min_gran, lim->max_gran);

	return (a_grn < lat) ? a_grn : lat;
}

PURE q16_t pg_cpu_calc_wakeup(q16_t p_eff,
			      const struct pg_cpu_eff *RESTRICT eff,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t dec = pg_math_decay(p_eff, eff->decay);
	q16_t rng = lim->max_wake - lim->min_wake;
	q16_t r_wake = lim->min_wake + q16_mul(rng, dec);
	return pg_math_clamp(r_wake, lim->min_wake, lim->max_wake);
}

PURE q16_t pg_cpu_calc_migration(q16_t vel, q16_t p_eff,
				 const struct pg_cpu_lim *lim)
{
	q16_t x = pg_math_clamp(q16_div(p_eff, INT_TO_Q16(100)), 0, Q16_ONE);
	q16_t fac = Q16_ONE - x;
	q16_t crv = q16_mul(fac, fac);
	q16_t rng = lim->max_mig - lim->min_mig;
	q16_t r_mig = lim->min_mig + q16_mul(rng, crv);

	q16_t a_vel = abs_q16(vel);
	q16_t v_rat = pg_math_clamp(q16_div(a_vel, INT_TO_Q16(25)), 0, Q16_ONE);
	q16_t mig = q16_mul(r_mig, Q16_ONE - q16_mul(v_rat, Q16_HALF));

	return pg_math_clamp(mig, lim->min_mig, lim->max_mig);
}

PURE q16_t pg_cpu_calc_walt(q16_t press, const struct pg_cpu_lim *lim)
{
	q16_t rat = q16_div(press, INT_TO_Q16(100));
	q16_t crv = q16_mul(rat, rat);
	q16_t rng = lim->max_walt - lim->min_walt;
	q16_t val = lim->min_walt + q16_mul(rng, crv);
	return pg_math_clamp(val, lim->min_walt, lim->max_walt);
}

PURE q16_t pg_cpu_calc_uclamp(q16_t press, q16_t th_scl,
			      const struct pg_cpu_cfg *RESTRICT cfg,
			      const struct pg_cpu_eff *RESTRICT eff,
			      const struct pg_cpu_lim *RESTRICT lim)
{
	q16_t sig = pg_math_sigmoid(press, cfg->uclamp_k, eff->ucl_mid);
	q16_t rng = lim->max_ucl - lim->min_ucl;
	q16_t ucl = lim->min_ucl + q16_mul(rng, sig);
	return pg_math_clamp(q16_mul(ucl, th_scl), lim->min_ucl, lim->max_ucl);
}
