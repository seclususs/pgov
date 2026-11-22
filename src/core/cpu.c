// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/cpu.h"

PURE bool cpu_transient(const struct cpu_state *state,
			const struct cpu_param *param)
{
	bool diff;
	bool rate;

	if (UNLIKELY(!state || !param))
		return false;

	rate = math_abs_q16(state->rate) > param->trans_rate;
	diff = math_abs_q16(state->psi_val - state->psi_ref) >
	       param->trans_diff;

	return (rate || diff) != 0;
}

void cpu_update_integral(struct cpu_state *RESTRICT state, q16_t bat_level,
			 q16_t dt_real, q16_t *RESTRICT integral,
			 q16_t *RESTRICT integral_dt)
{
	q16_t curve;
	q16_t delta;
	q16_t diff;
	q16_t level;
	q16_t norm;
	q16_t rate = 0;
	q16_t sq;
	q16_t target;
	q16_t tau;

	if (UNLIKELY(!state || !integral || !integral_dt))
		return;

	level = math_clamp(bat_level, 0, INT_TO_Q16(100));
	delta = INT_TO_Q16(100) - level;
	norm = math_div_q16(delta, INT_TO_Q16(100));
	sq = math_mul_q16(norm, norm);
	curve = math_mul_q16(sq, norm);
	target = math_mul_q16(FLOAT_TO_Q16(97.0F), curve);

	if (UNLIKELY(state->first_run)) {
		state->prev_integral = target;
		state->first_run = false;
		*integral = target;
		*integral_dt = 0;
		return;
	}

	if (LIKELY(dt_real > 0)) {
		diff = target - state->prev_integral;
		tau = INT_TO_Q16(5);
		rate = math_div_q16(diff, tau);

		if (LIKELY(dt_real < tau))
			target = state->prev_integral +
				 math_mul_q16(rate, dt_real);
	}

	state->prev_integral = target;
	*integral = target;
	*integral_dt = rate;
}

static inline q32_t calculate_loss(q16_t integral, q16_t psi_val, q16_t k_final)
{
	q32_t limit;
	q32_t loss;
	q32_t resp;

	loss = math_mul_q32(Q16_TO_Q32(integral), Q16_TO_Q32(psi_val));
	resp = math_mul_q32(Q16_TO_Q32(k_final), Q16_TO_Q32(INT_TO_Q16(100)));
	limit = math_mul_q32(resp, Q16_TO_Q32(FLOAT_TO_Q16(1.5F)));

	return (loss > limit) ? limit : loss;
}

static inline q16_t calculate_damping(q16_t k_final, q16_t velocity,
				      q16_t integral_dt, q16_t psi_val,
				      q16_t therm_scale,
				      const struct cpu_efficiency *efficiency)
{
	q16_t c_base;
	q16_t c_crit;
	q16_t c_max;
	q32_t c_req;
	q16_t c_stab;
	q16_t c_therm;
	q16_t clamped;
	q32_t num;
	q32_t rate;
	q16_t root;
	q32_t v;
	q32_t v_sq;
	q32_t x;
	q32_t x_sq;

	c_crit = math_mul_q16(INT_TO_Q16(2), math_sqrt_q16(k_final));
	c_base = math_mul_q16(c_crit, efficiency->stab_ratio);
	v = Q16_TO_Q32(velocity);
	v_sq = math_mul_q32(v, v) + Q16_TO_Q32(FLOAT_TO_Q16(0.001F));

	rate = Q16_TO_Q32(math_mul_q16(Q16_HALF, math_abs_q16(integral_dt)));
	x = Q16_TO_Q32(psi_val);
	x_sq = math_mul_q32(x, x);
	num = math_mul_q32(rate, x_sq);

	c_req = (v_sq > 0) ? num / (v_sq >> Q16_SHIFT) : 0;
	c_max = math_mul_q16(c_base, INT_TO_Q16(4));

	if (c_req >= c_max)
		clamped = c_max;
	else if (c_req <= 0)
		clamped = 0;
	else
		clamped = (q16_t)c_req;

	c_stab = math_mul_q16(clamped, FLOAT_TO_Q16(3.1F));

	root = math_sqrt_q16(therm_scale);
	c_therm = (root > 0) ? math_div_q16(c_base, root) : c_base;

	return (c_therm > c_stab) ? c_therm : c_stab;
}

void cpu_update_efficiency(struct cpu_efficiency *efficiency, q16_t bat_level,
			   q16_t therm_scale, q16_t avg300)
{
	q16_t factor;
	q16_t health;
	q16_t level;
	q16_t noise;

	if (UNLIKELY(!efficiency))
		return;

	level = math_clamp(bat_level, 0, INT_TO_Q16(100));
	factor = math_div_q16(level, INT_TO_Q16(100));
	health = math_mul_q16(factor, therm_scale);

	efficiency->resp_gain = math_mul_q16(FLOAT_TO_Q16(31.0F), health);
	efficiency->surge_gain = math_mul_q16(FLOAT_TO_Q16(0.095F), health);
	efficiency->trend_amp = math_mul_q16(FLOAT_TO_Q16(0.115F), health);
	efficiency->lookahead = math_mul_q16(FLOAT_TO_Q16(0.17F), therm_scale);
	efficiency->decay = math_mul_q16(FLOAT_TO_Q16(0.019F), therm_scale);

	efficiency->sigmoid_mid =
		FLOAT_TO_Q16(6.8F) + math_mul_q16(FLOAT_TO_Q16(0.5F), avg300);

	efficiency->uclamp_mid = efficiency->sigmoid_mid + FLOAT_TO_Q16(2.7F);

	noise = math_mul_q16(avg300, FLOAT_TO_Q16(0.1F));
	noise = math_clamp(noise, 0, Q16_ONE);

	efficiency->nis_thresh =
		math_lerp(FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(12.0F), noise);

	efficiency->stab_ratio =
		math_lerp(FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(3.5F), noise);

	efficiency->gain_alpha =
		math_lerp(FLOAT_TO_Q16(0.99F), FLOAT_TO_Q16(0.85F), noise);
}

q16_t cpu_calc_load_demand(struct cpu_state *state,
			   const struct cpu_demand *demand,
			   const struct cpu_param *param)
{
	q16_t c;
	q16_t den;
	q16_t disp;
	q16_t dt;
	struct cpu_efficiency *eff;
	q32_t f_loss;
	q32_t f_net;
	q32_t f_spring;
	q16_t k_dyn;
	q16_t k_final;
	q16_t pred;
	q16_t th;
	q16_t trend;
	q16_t v;

	if (UNLIKELY(!state || !demand || !param))
		return 0;

	eff = &state->efficiency;

	if (UNLIKELY(demand->target_psi < INT_TO_Q16(1) &&
		     math_abs_q16(demand->velocity) < FLOAT_TO_Q16(0.01F) &&
		     state->psi_val < INT_TO_Q16(1) &&
		     math_abs_q16(state->rate) < FLOAT_TO_Q16(0.01F))) {
		state->psi_val = 0;
		state->rate = 0;
		return 0;
	}

	if (UNLIKELY(demand->struct_break)) {
		state->psi_val = demand->target_psi;
		state->rate = 0;
	}

	v = demand->velocity;
	if (math_abs_q16(v) > param->surge_thresh)
		state->rate += math_mul_q16(v, eff->surge_gain);

	pred = demand->target_psi + math_mul_q16(v, eff->lookahead);
	trend = math_mul_q16(eff->gain_alpha, demand->trend_factor);
	k_dyn = math_mul_q16(eff->resp_gain, Q16_ONE + trend);
	th = math_clamp(demand->therm_scale, FLOAT_TO_Q16(0.1F), Q16_ONE);

	k_final = math_mul_q16(k_dyn, math_mul_q16(th, th));
	disp = pred - state->psi_val;
	f_spring = math_mul_q32(Q16_TO_Q32(k_final), Q16_TO_Q32(disp));

	f_loss = calculate_loss(demand->integral, state->psi_val, k_final);
	c = calculate_damping(k_final, v, demand->integral_dt, state->psi_val,
			      th, eff);

	f_net = f_spring - f_loss;

	if (f_net > Q16_TO_Q32(INT_TO_Q16(32000)))
		f_net = Q16_TO_Q32(INT_TO_Q16(32000));

	else if (f_net < Q16_TO_Q32(INT_TO_Q16(-32000)))
		f_net = Q16_TO_Q32(INT_TO_Q16(-32000));

	dt = demand->dt_safe;
	den = Q16_ONE + math_mul_q16(c, dt);
	state->rate = math_div_q16(
		state->rate + math_mul_q16(Q32_TO_Q16(f_net), dt), den);

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

PURE q16_t cpu_calc_trend_gain(q16_t velocity)
{
	if (velocity > 0)
		return math_algebraic_sigmoid(velocity);

	return 0;
}

PURE q16_t cpu_calc_effective_pressure(q16_t demand, q16_t trend_factor,
				       const struct cpu_efficiency *efficiency)
{
	q16_t scale;

	if (UNLIKELY(!efficiency))
		return demand;

	scale = Q16_ONE + math_mul_q16(trend_factor, efficiency->trend_amp);

	return math_mul_q16(demand, scale);
}

PURE q16_t cpu_calc_thermal_latency(q16_t therm_scale,
				    const struct cpu_limit *limit)
{
	q16_t range;
	q16_t ratio;

	if (UNLIKELY(!limit))
		return 0;

	ratio = math_clamp(Q16_ONE - therm_scale, 0, Q16_ONE);
	range = limit->max_lat - limit->min_lat;

	return limit->min_lat + math_mul_q16(range, ratio);
}

PURE q16_t cpu_calc_latency(q16_t pressure, q16_t demand, q16_t therm_lat,
			    const struct cpu_param *param,
			    const struct cpu_efficiency *efficiency,
			    const struct cpu_limit *limit)
{
	q16_t final;
	q16_t inv;
	q16_t load;
	q16_t nom;
	q16_t norm;
	q16_t range;
	q16_t sig;
	q16_t target;

	if (UNLIKELY(!param || !efficiency || !limit))
		return 0;

	sig = math_sigmoid(pressure, param->sigmoid_k, efficiency->sigmoid_mid);
	inv = Q16_ONE - sig;
	range = limit->max_lat - limit->min_lat;
	nom = limit->min_lat + math_mul_q16(range, inv);
	norm = math_clamp(math_div_q16(demand, INT_TO_Q16(100)), 0, Q16_ONE);

	load = limit->max_lat - math_mul_q16(norm, range);
	target = (nom < load) ? nom : load;
	final = (target > therm_lat) ? target : therm_lat;

	return math_clamp(final, limit->min_lat, limit->max_lat);
}

PURE q16_t cpu_calc_granularity(q16_t latency, const struct cpu_param *param,
				const struct cpu_limit *limit)
{
	q16_t scaled;
	q16_t target;

	if (UNLIKELY(!param || !limit))
		return latency;

	scaled = math_mul_q16(latency, param->lat_gran_rat);
	target = math_clamp(scaled, limit->min_gran, limit->max_gran);

	return (target < latency) ? target : latency;
}

PURE q16_t cpu_calc_wakeup(q16_t pressure,
			   const struct cpu_efficiency *efficiency,
			   const struct cpu_limit *limit)
{
	q16_t decay;
	q16_t range;
	q16_t target;

	if (UNLIKELY(!efficiency || !limit))
		return 0;

	decay = math_decay(pressure, efficiency->decay);
	range = limit->max_wake - limit->min_wake;
	target = limit->min_wake + math_mul_q16(range, decay);

	return math_clamp(target, limit->min_wake, limit->max_wake);
}

PURE q16_t cpu_calc_migration(q16_t velocity, q16_t pressure,
			      const struct cpu_limit *limit)
{
	q16_t base;
	q16_t curve;
	q16_t inv;
	q16_t norm;
	q16_t range;
	q16_t ratio;
	q16_t target;
	q16_t v;

	if (UNLIKELY(!limit))
		return 0;

	norm = math_clamp(math_div_q16(pressure, INT_TO_Q16(100)), 0, Q16_ONE);
	inv = Q16_ONE - norm;
	curve = math_mul_q16(inv, inv);
	range = limit->max_mig - limit->min_mig;
	base = limit->min_mig + math_mul_q16(range, curve);

	v = math_abs_q16(velocity);
	ratio = math_clamp(math_div_q16(v, INT_TO_Q16(25)), 0, Q16_ONE);
	target = math_mul_q16(base, Q16_ONE - math_mul_q16(ratio, Q16_HALF));

	return math_clamp(target, limit->min_mig, limit->max_mig);
}

PURE q16_t cpu_calc_walt(q16_t pressure, const struct cpu_limit *limit)
{
	q16_t curve;
	q16_t norm;
	q16_t range;
	q16_t target;

	if (UNLIKELY(!limit))
		return 0;

	norm = math_clamp(math_div_q16(pressure, INT_TO_Q16(100)), 0, Q16_ONE);
	curve = math_mul_q16(norm, norm);
	range = limit->max_walt - limit->min_walt;
	target = limit->min_walt + math_mul_q16(range, curve);

	return math_clamp(target, limit->min_walt, limit->max_walt);
}

PURE q16_t cpu_calc_uclamp_min(q16_t pressure, q16_t therm_scale,
			       const struct cpu_param *param,
			       const struct cpu_efficiency *efficiency,
			       const struct cpu_limit *limit)
{
	q16_t range;
	q16_t scaled;
	q16_t sig;
	q16_t target;

	if (UNLIKELY(!param || !efficiency || !limit))
		return 0;

	sig = math_sigmoid(pressure, param->uclamp_k, efficiency->uclamp_mid);
	range = limit->max_ucl - limit->min_ucl;
	target = limit->min_ucl + math_mul_q16(range, sig);
	scaled = math_mul_q16(target, therm_scale);

	return math_clamp(scaled, limit->min_ucl, limit->max_ucl);
}

PURE q16_t cpu_calc_uclamp_max(q16_t therm_scale, q16_t bat_level,
			       const struct cpu_limit *limit)
{
	q16_t ceiling;
	q16_t clamped;
	q16_t factor;
	q16_t health = Q16_ONE;
	q16_t level;
	q16_t norm;
	q16_t range;
	q16_t target;

	if (UNLIKELY(!limit))
		return INT_TO_Q16(CPU_UCLAMP_MAX_CEILING);

	level = math_clamp(bat_level, 0, INT_TO_Q16(100));

	if (therm_scale < FLOAT_TO_Q16(0.80F)) {
		clamped = math_clamp(therm_scale, FLOAT_TO_Q16(0.10F),
				     FLOAT_TO_Q16(0.80F));

		norm = math_div_q16(clamped - FLOAT_TO_Q16(0.10F),
				    FLOAT_TO_Q16(0.70F));

		health = math_mul_q16(health, norm);
	}

	if (level < INT_TO_Q16(15)) {
		factor = math_div_q16(level, INT_TO_Q16(15));
		health = math_mul_q16(health, factor);
	}

	if (health >= Q16_ONE)
		return INT_TO_Q16(CPU_UCLAMP_MAX_CEILING);

	ceiling = INT_TO_Q16(CPU_UCLAMP_MAX_CEILING);
	range = ceiling - limit->ucl_max;
	target = limit->ucl_max + math_mul_q16(range, health);

	return math_clamp(target, limit->ucl_max, ceiling);
}
