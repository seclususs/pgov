// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/cpu.h"

PURE bool pg_cpu_trans(const struct pg_load_state *state,
		       const struct pg_cpu_cfg *cfg)
{
	if (UNLIKELY(!state || !cfg))
		return false;

	bool rate = math_abs_q16(state->rate) > cfg->trans_rate;
	bool diff = math_abs_q16(state->psi_val - state->psi_ref) >
		    cfg->trans_diff;

	return (rate || diff) != 0;
}

void pg_cpu_upd_intg(struct pg_load_state *RESTRICT state, q16_t bat_lvl,
		     q16_t dt_real, q16_t *RESTRICT i, q16_t *RESTRICT i_dt)
{
	if (UNLIKELY(!state || !i || !i_dt))
		return;

	q16_t b_lvl = math_clamp(bat_lvl, 0, INT_TO_Q16(100));
	q16_t d_bat = INT_TO_Q16(100) - b_lvl;
	q16_t depl = math_div_q16(d_bat, INT_TO_Q16(100));
	q16_t depl2 = math_mul_q16(depl, depl);
	q16_t depl3 = math_mul_q16(depl2, depl);
	q16_t c_intg = math_mul_q16(FLOAT_TO_Q16(97.0F), depl3);

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
		c_dt = math_div_q16(diff, tau);
	}

	state->prev_integ = c_intg;
	*i = c_intg;
	*i_dt = c_dt;
}

static inline q32_t calc_l_term(q16_t integ, q16_t psi_val, q16_t k_fin)
{
	q32_t l_term = math_mul_q32(Q16_TO_Q32(integ), Q16_TO_Q32(psi_val));
	q32_t m_rsp =
		math_mul_q32(Q16_TO_Q32(k_fin), Q16_TO_Q32(INT_TO_Q16(100)));

	q32_t lim_l = math_mul_q32(m_rsp, Q16_TO_Q32(FLOAT_TO_Q16(1.5F)));
	return (l_term > lim_l) ? lim_l : l_term;
}

static inline q16_t calc_c_final(q16_t k_fin, q16_t vel, q16_t i_dt,
				 q16_t psi_val, q16_t th,
				 const struct pg_cpu_eff *eff)
{
	q16_t c_crit = math_mul_q16(INT_TO_Q16(2), math_sqrt_q16(k_fin));
	q16_t c_base = math_mul_q16(c_crit, eff->stab_rat);
	q32_t load = Q16_TO_Q32(vel);
	q32_t r_sq =
		math_mul_q32(load, load) + Q16_TO_Q32(FLOAT_TO_Q16(0.001F));

	q32_t d_n1 = Q16_TO_Q32(math_mul_q16(Q16_HALF, math_abs_q16(i_dt)));
	q32_t p_val = Q16_TO_Q32(psi_val);
	q32_t d_n2 = math_mul_q32(p_val, p_val);
	q32_t d_num = math_mul_q32(d_n1, d_n2);

	q32_t c_req = (r_sq > 0) ? d_num / (r_sq >> Q16_SHIFT) : 0;
	q16_t c_max = math_mul_q16(c_base, INT_TO_Q16(4));
	q16_t c_clp;

	if (c_req >= c_max)
		c_clp = c_max;

	else if (c_req <= 0)
		c_clp = 0;
	else
		c_clp = (q16_t)c_req;

	q16_t c_stb = math_mul_q16(c_clp, FLOAT_TO_Q16(3.1F));

	q16_t th_sq = math_sqrt_q16(th);
	q16_t c_th = (th_sq > 0) ? math_div_q16(c_base, th_sq) : c_base;

	return (c_th > c_stb) ? c_th : c_stb;
}

void pg_cpu_upd_eff(struct pg_cpu_eff *eff, q16_t bat_lvl, q16_t th_scl,
		    q16_t avg300)
{
	if (UNLIKELY(!eff))
		return;

	q16_t b_lvl = math_clamp(bat_lvl, 0, INT_TO_Q16(100));
	q16_t bat_fact = math_div_q16(b_lvl, INT_TO_Q16(100));
	q16_t health = math_mul_q16(bat_fact, th_scl);

	eff->resp_gain = math_mul_q16(FLOAT_TO_Q16(31.0F), health);
	eff->surge_gain = math_mul_q16(FLOAT_TO_Q16(0.095F), health);
	eff->trend_amp = math_mul_q16(FLOAT_TO_Q16(0.115F), health);
	eff->lookahead = math_mul_q16(FLOAT_TO_Q16(0.17F), th_scl);
	eff->decay = math_mul_q16(FLOAT_TO_Q16(0.019F), th_scl);
	eff->sig_mid =
		FLOAT_TO_Q16(6.8F) + math_mul_q16(FLOAT_TO_Q16(0.5F), avg300);
	eff->ucl_mid = eff->sig_mid + FLOAT_TO_Q16(2.7F);

	q16_t noise = math_mul_q16(avg300, FLOAT_TO_Q16(0.1F));
	noise = math_clamp(noise, 0, Q16_ONE);

	eff->nis_thresh =
		math_lerp(FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(12.0F), noise);
	eff->stab_rat =
		math_lerp(FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(3.5F), noise);
	eff->gain_alpha =
		math_lerp(FLOAT_TO_Q16(0.99F), FLOAT_TO_Q16(0.85F), noise);
}

q16_t pg_cpu_calc_load_demand(struct pg_load_state *state,
			      const struct pg_demand_input *input,
			      const struct pg_cpu_cfg *cfg)
{
	if (UNLIKELY(!state || !input || !cfg))
		return 0;

	struct pg_cpu_eff *eff = &state->eff;

	if (UNLIKELY(input->tgt_psi < INT_TO_Q16(1) &&
		     math_abs_q16(input->vel) < FLOAT_TO_Q16(0.01F) &&
		     state->psi_val < INT_TO_Q16(1) &&
		     math_abs_q16(state->rate) < FLOAT_TO_Q16(0.01F))) {
		state->psi_val = 0;
		state->rate = 0;
		return 0;
	}

	if (UNLIKELY(input->struct_break)) {
		state->psi_val = input->tgt_psi;
		state->rate = 0;
	}

	q16_t l_rate = input->vel;
	if (math_abs_q16(l_rate) > cfg->surge_thresh)
		state->rate += math_mul_q16(l_rate, eff->surge_gain);

	q16_t pred = input->tgt_psi + math_mul_q16(l_rate, eff->lookahead);
	q16_t trend = math_mul_q16(eff->gain_alpha, input->trend_fact);
	q16_t k_dyn = math_mul_q16(eff->resp_gain, Q16_ONE + trend);
	q16_t th = math_clamp(input->therm_scale, FLOAT_TO_Q16(0.1F), Q16_ONE);

	q16_t k_fin = math_mul_q16(k_dyn, math_mul_q16(th, th));
	q16_t disp = pred - state->psi_val;
	q32_t p_term = math_mul_q32(Q16_TO_Q32(k_fin), Q16_TO_Q32(disp));

	q32_t l_term = calc_l_term(input->integ, state->psi_val, k_fin);
	q16_t c_fin = calc_c_final(k_fin, l_rate, input->integ_dt,
				   state->psi_val, th, eff);

	q32_t f_q32 = p_term - l_term;

	if (f_q32 > Q16_TO_Q32(INT_TO_Q16(32000)))
		f_q32 = Q16_TO_Q32(INT_TO_Q16(32000));

	else if (f_q32 < Q16_TO_Q32(INT_TO_Q16(-32000)))
		f_q32 = Q16_TO_Q32(INT_TO_Q16(-32000));

	q16_t dt = input->dt_safe;
	q16_t den = Q16_ONE + math_mul_q16(c_fin, dt);
	state->rate = math_div_q16(
		state->rate + math_mul_q16(Q32_TO_Q16(f_q32), dt), den);

	state->psi_val += math_mul_q16(state->rate, dt);

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
		return math_algebraic_sigmoid(vel);

	return 0;
}

PURE q16_t pg_cpu_calc_eff_press(q16_t l_dem, q16_t t_fact,
				 const struct pg_cpu_eff *eff)
{
	if (UNLIKELY(!eff))
		return l_dem;

	q16_t t_mul = Q16_ONE + math_mul_q16(t_fact, eff->trend_amp);
	return math_mul_q16(l_dem, t_mul);
}

PURE q16_t pg_cpu_calc_therm_lat(q16_t th_scl, const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!lim))
		return 0;

	q16_t l_rat = math_clamp(Q16_ONE - th_scl, 0, Q16_ONE);
	q16_t l_rng = lim->max_lat - lim->min_lat;
	return lim->min_lat + math_mul_q16(l_rng, l_rat);
}

PURE q16_t pg_cpu_calc_lat(q16_t p_eff, q16_t l_dem, q16_t th_lat,
			   const struct pg_cpu_cfg *cfg,
			   const struct pg_cpu_eff *eff,
			   const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!cfg || !eff || !lim))
		return 0;

	q16_t sig = math_sigmoid(p_eff, cfg->sigmoid_k, eff->sig_mid);
	q16_t fac = Q16_ONE - sig;
	q16_t rng = lim->max_lat - lim->min_lat;
	q16_t n_lat = lim->min_lat + math_mul_q16(rng, fac);
	q16_t e_dem =
		math_clamp(math_div_q16(l_dem, INT_TO_Q16(100)), 0, Q16_ONE);

	q16_t l_lat = lim->max_lat - math_mul_q16(e_dem, rng);
	q16_t i_lat = (n_lat < l_lat) ? n_lat : l_lat;
	q16_t f_lat = (i_lat > th_lat) ? i_lat : th_lat;

	return math_clamp(f_lat, lim->min_lat, lim->max_lat);
}

PURE q16_t pg_cpu_calc_gran(q16_t lat, const struct pg_cpu_cfg *cfg,
			    const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!cfg || !lim))
		return lat;

	q16_t r_grn = math_mul_q16(lat, cfg->lat_gran_rat);
	q16_t a_grn = math_clamp(r_grn, lim->min_gran, lim->max_gran);

	return (a_grn < lat) ? a_grn : lat;
}

PURE q16_t pg_cpu_calc_wakeup(q16_t p_eff, const struct pg_cpu_eff *eff,
			      const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!eff || !lim))
		return 0;

	q16_t dec = math_decay(p_eff, eff->decay);
	q16_t rng = lim->max_wake - lim->min_wake;
	q16_t r_wake = lim->min_wake + math_mul_q16(rng, dec);
	return math_clamp(r_wake, lim->min_wake, lim->max_wake);
}

PURE q16_t pg_cpu_calc_migration(q16_t vel, q16_t p_eff,
				 const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!lim))
		return 0;

	q16_t x = math_clamp(math_div_q16(p_eff, INT_TO_Q16(100)), 0, Q16_ONE);
	q16_t fac = Q16_ONE - x;
	q16_t crv = math_mul_q16(fac, fac);
	q16_t rng = lim->max_mig - lim->min_mig;
	q16_t r_mig = lim->min_mig + math_mul_q16(rng, crv);

	q16_t a_vel = math_abs_q16(vel);
	q16_t v_rat =
		math_clamp(math_div_q16(a_vel, INT_TO_Q16(25)), 0, Q16_ONE);
	q16_t mig =
		math_mul_q16(r_mig, Q16_ONE - math_mul_q16(v_rat, Q16_HALF));

	return math_clamp(mig, lim->min_mig, lim->max_mig);
}

PURE q16_t pg_cpu_calc_walt(q16_t press, const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!lim))
		return 0;

	q16_t rat =
		math_clamp(math_div_q16(press, INT_TO_Q16(100)), 0, Q16_ONE);
	q16_t crv = math_mul_q16(rat, rat);
	q16_t rng = lim->max_walt - lim->min_walt;
	q16_t val = lim->min_walt + math_mul_q16(rng, crv);
	return math_clamp(val, lim->min_walt, lim->max_walt);
}

PURE q16_t pg_cpu_calc_uclamp(q16_t press, q16_t th_scl,
			      const struct pg_cpu_cfg *cfg,
			      const struct pg_cpu_eff *eff,
			      const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!cfg || !eff || !lim))
		return 0;

	q16_t sig = math_sigmoid(press, cfg->uclamp_k, eff->ucl_mid);
	q16_t rng = lim->max_ucl - lim->min_ucl;
	q16_t ucl = lim->min_ucl + math_mul_q16(rng, sig);
	return math_clamp(math_mul_q16(ucl, th_scl), lim->min_ucl,
			  lim->max_ucl);
}

PURE q16_t pg_cpu_calc_uclamp_max(q16_t th_scl, q16_t bat_lvl,
				  const struct pg_cpu_lim *lim)
{
	if (UNLIKELY(!lim))
		return INT_TO_Q16(UCL_MAX_CEILING);

	q16_t b_lvl = math_clamp(bat_lvl, 0, INT_TO_Q16(100));
	q16_t th_norm = Q16_ONE;

	if (th_scl < FLOAT_TO_Q16(0.80F)) {
		q16_t clamped_th = math_clamp(th_scl, FLOAT_TO_Q16(0.10F),
					      FLOAT_TO_Q16(0.80F));
		th_norm = math_div_q16(clamped_th - FLOAT_TO_Q16(0.10F),
				       FLOAT_TO_Q16(0.70F));
	}

	q16_t bat_norm = Q16_ONE;
	if (b_lvl < INT_TO_Q16(15))
		bat_norm = math_div_q16(b_lvl, INT_TO_Q16(15));

	q16_t health = math_mul_q16(th_norm, bat_norm);
	if (health >= Q16_ONE)
		return INT_TO_Q16(UCL_MAX_CEILING);

	q16_t ceiling = INT_TO_Q16(UCL_MAX_CEILING);
	q16_t rng = ceiling - lim->min_ucl_max;
	q16_t val = lim->min_ucl_max + math_mul_q16(rng, health);

	return math_clamp(val, lim->min_ucl_max, ceiling);
}
