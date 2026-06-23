// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/cpu.h"
#include "pascal_gov/math.h"
#include <math.h>

bool pascal_gov_cpu_is_transient(
	const pascal_gov_load_state *PASCAL_GOV_RESTRICT state,
	float target_psi,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config)
{
	bool rate_exceeded =
		fabsf(state->rate) > math_config->transient_rate_threshold;

	bool diff_exceeded = fabsf(state->psi_value - target_psi) >
			     math_config->transient_diff_threshold;

	return (rate_exceeded || diff_exceeded) != 0;
}

void pascal_gov_cpu_update_integral_params(
	pascal_gov_load_state *PASCAL_GOV_RESTRICT state, float bat_level,
	float dt_safe,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	float *PASCAL_GOV_RESTRICT out_integral_total,
	float *PASCAL_GOV_RESTRICT out_integral_dot)
{
	float depletion = fmaxf(100.0F - bat_level, 0.0F) / 100.0F;
	float depletion_cubed = depletion * depletion * depletion;
	float cost_heuristic = math_config->bat_level_weight * depletion_cubed;
	float total_integral = cost_heuristic;

	if (PASCAL_GOV_UNLIKELY(state->first_run)) {
		state->prev_integral = total_integral;
		state->first_run = false;
		*out_integral_total = total_integral;
		*out_integral_dot = 0.0F;
		return;
	}

	float integral_dot = 0.0F;
	if (PASCAL_GOV_LIKELY(dt_safe > 0.0F)) {
		float delta_integral = total_integral - state->prev_integral;
		integral_dot = delta_integral / dt_safe;
	}

	state->prev_integral = total_integral;

	*out_integral_total = total_integral;
	*out_integral_dot = integral_dot;
}

float pascal_gov_cpu_calculate_load_demand(
	pascal_gov_load_state *PASCAL_GOV_RESTRICT state,
	const pascal_gov_demand_input *PASCAL_GOV_RESTRICT input,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config)
{
	if (PASCAL_GOV_UNLIKELY(input->is_structural_break)) {
		state->psi_value = input->target_psi;
		state->rate = 0.0F;
	}

	float load_rate = input->psi_velocity;
	if (fabsf(load_rate) > math_config->surge_threshold)
		state->rate += load_rate * math_config->surge_gain;

	float prediction_target =
		input->target_psi + (load_rate * math_config->lookahead_time);

	float trend_impact =
		math_config->gain_scheduling_alpha * input->trend_factor;

	float k_dynamic = math_config->response_gain * (1.0F + trend_impact);

	float clamped_thermal =
		pascal_gov_clampf(input->thermal_scale, 0.1F, 1.0F);

	float k_final = k_dynamic * (clamped_thermal * clamped_thermal);

	float displacement = prediction_target - state->psi_value;
	float prop_term = k_final * displacement;

	float limit_term = input->integral_total * state->psi_value;
	float max_possible_response = k_final * 100.0F;
	limit_term = fminf(limit_term, max_possible_response * 1.5F);

	float crit_damp = 2.0F * sqrtf(k_final);
	float base_damp = crit_damp * math_config->stability_ratio;

	float rate_sq = (load_rate * load_rate) + 0.001F;

	float damping_numerator = 0.5F * fabsf(input->integral_dot) *
				  (state->psi_value * state->psi_value);

	float stability_damping_req = damping_numerator / rate_sq;

	float max_damping = base_damp * 4.0F;
	float clamped_damping =
		pascal_gov_clampf(stability_damping_req, 0.0F, max_damping);

	float c_stability = clamped_damping * math_config->stability_margin;

	float c_thermal_adjusted = base_damp / sqrtf(clamped_thermal);
	float c_final = fmaxf(c_thermal_adjusted, c_stability);
	float deriv_term = c_final * state->rate;

	float net_correction = prop_term - deriv_term - limit_term;
	float rate_delta = net_correction;

	state->rate += rate_delta * input->dt_safe;
	state->psi_value += state->rate * input->dt_real;

	if (PASCAL_GOV_UNLIKELY(state->psi_value < 0.0F)) {
		state->psi_value = 0.0F;
		state->rate = 0.0F;
	} else if (PASCAL_GOV_UNLIKELY(state->psi_value > 500.0F)) {
		state->psi_value = 500.0F;
		state->rate = 0.0F;
	}

	return state->psi_value;
}

float pascal_gov_cpu_calculate_trend_gain(float velocity)
{
	if (velocity > 0.0F)
		return pascal_gov_tanh_approx(velocity);

	return 0.0F;
}

float pascal_gov_cpu_calculate_effective_pressure(
	float load_demand, float trend_factor,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config)
{
	float trend_multiplier =
		1.0F + (trend_factor * math_config->trend_amplification);

	return load_demand * trend_multiplier;
}

float pascal_gov_cpu_calculate_thermal_latency_limit(
	float thermal_scale,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits)
{
	float limit_ratio = pascal_gov_clampf(1.0F - thermal_scale, 0.0F, 1.0F);
	float latency_range =
		kernel_limits->max_latency_ns - kernel_limits->min_latency_ns;

	return kernel_limits->min_latency_ns + (latency_range * limit_ratio);
}

void pascal_gov_cpu_calculate_latency_and_granularity(
	float p_eff, float load_demand, float thermal_min_latency_ns,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits,
	float *PASCAL_GOV_RESTRICT out_latency,
	float *PASCAL_GOV_RESTRICT out_granularity)
{
	float sigmoid_val = pascal_gov_sigmoid_param(
		p_eff, math_config->sigmoid_k, math_config->sigmoid_mid);

	float factor = 1.0F - sigmoid_val;

	float latency_range =
		kernel_limits->max_latency_ns - kernel_limits->min_latency_ns;

	float normal_latency =
		kernel_limits->min_latency_ns + (latency_range * factor);

	float effective_demand =
		pascal_gov_clampf(load_demand / 100.0F, 0.0F, 1.0F);

	float low_latency_target = kernel_limits->max_latency_ns -
				   (effective_demand * latency_range);

	float ideal_latency = fminf(normal_latency, low_latency_target);
	float final_latency = fmaxf(ideal_latency, thermal_min_latency_ns);

	float adjusted_latency =
		pascal_gov_clampf(final_latency, kernel_limits->min_latency_ns,
				  kernel_limits->max_latency_ns);

	float raw_gran = adjusted_latency * math_config->latency_gran_ratio;
	float clamped_gran =
		pascal_gov_clampf(raw_gran, kernel_limits->min_granularity_ns,
				  kernel_limits->max_granularity_ns);

	float final_gran = fminf(clamped_gran, adjusted_latency);

	*out_latency = adjusted_latency;
	*out_granularity = final_gran;
}

float pascal_gov_cpu_calculate_wakeup_granularity(
	float p_eff,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits)
{
	float decay = pascal_gov_decay(p_eff, math_config->decay_coeff);
	float wakeup_range =
		kernel_limits->max_wakeup_ns - kernel_limits->min_wakeup_ns;

	float raw_wake = kernel_limits->min_wakeup_ns + (wakeup_range * decay);

	return pascal_gov_clampf(raw_wake, kernel_limits->min_wakeup_ns,
				 kernel_limits->max_wakeup_ns);
}

float pascal_gov_cpu_calculate_migration_cost(
	float velocity, float p_eff,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits)
{
	float x = pascal_gov_clampf(p_eff / 100.0F, 0.0F, 1.0F);
	float factor = 1.0F - x;
	float curve = factor * factor;

	float mig_range = kernel_limits->max_migration_cost -
			  kernel_limits->min_migration_cost;

	float raw_mig = kernel_limits->min_migration_cost + (mig_range * curve);

	float volatility_ratio =
		pascal_gov_clampf(fabsf(velocity) / 25.0F, 0.0F, 1.0F);

	float dynamic_cost = raw_mig * (1.0F - (volatility_ratio * 0.5F));

	return pascal_gov_clampf(dynamic_cost,
				 kernel_limits->min_migration_cost,
				 kernel_limits->max_migration_cost);
}

float pascal_gov_cpu_calculate_walt_init(
	float pressure,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits)
{
	float ratio = pressure / 100.0F;
	float load_curve = ratio * ratio;
	float range = kernel_limits->max_walt_init_pct -
		      kernel_limits->min_walt_init_pct;

	float val = kernel_limits->min_walt_init_pct + (range * load_curve);

	return pascal_gov_clampf(val, kernel_limits->min_walt_init_pct,
				 kernel_limits->max_walt_init_pct);
}

float pascal_gov_cpu_calculate_uclamp_min(
	float pressure, float thermal_scale,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits)
{
	float sigmoid_val = pascal_gov_sigmoid_param(
		pressure, math_config->uclamp_k, math_config->uclamp_mid);

	float range =
		kernel_limits->max_uclamp_min - kernel_limits->min_uclamp_min;

	float ideal_uclamp =
		kernel_limits->min_uclamp_min + (range * sigmoid_val);

	return pascal_gov_clampf(ideal_uclamp * thermal_scale,
				 kernel_limits->min_uclamp_min,
				 kernel_limits->max_uclamp_min);
}
