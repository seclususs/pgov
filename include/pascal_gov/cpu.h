// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_CPU_H
#define PASCAL_GOV_CPU_H

#include "pascal_gov/compiler.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	float min_latency_ns;
	float max_latency_ns;
	float min_granularity_ns;
	float max_granularity_ns;
	float min_wakeup_ns;
	float max_wakeup_ns;
	float min_migration_cost;
	float max_migration_cost;
	float min_walt_init_pct;
	float max_walt_init_pct;
	float min_uclamp_min;
	float max_uclamp_min;
} pascal_gov_cpu_limits;

typedef struct {
	float latency_gran_ratio;
	float decay_coeff;
	float uclamp_k;
	float uclamp_mid;
	float response_gain;
	float stability_ratio;
	float stability_margin;
	float gain_scheduling_alpha;
	float sigmoid_k;
	float sigmoid_mid;
	float lookahead_time;
	float trend_amplification;
	float surge_threshold;
	float surge_gain;
	float transient_rate_threshold;
	float transient_diff_threshold;
	float transient_poll_interval;
	float nis_threshold;
	float bat_level_weight;
} pascal_gov_cpu_math_config;

typedef struct {
	float poll_weight_pressure;
	float poll_weight_derivative;
	uint64_t battery_check_interval_sec;
	int32_t psi_threshold_us;
	int32_t psi_window_us;
} pascal_gov_controller_config;

typedef struct {
	float target_psi;
	float psi_velocity;
	float dt_real;
	float dt_safe;
	float thermal_scale;
	float trend_factor;
	float integral_total;
	float integral_dot;
	bool is_structural_break;
} pascal_gov_demand_input;

typedef struct {
	float psi_value;
	float rate;
	float prev_integral;
	bool first_run;
} pascal_gov_load_state;

__attribute__((pure)) bool pascal_gov_cpu_is_transient(
	const pascal_gov_load_state *PASCAL_GOV_RESTRICT state,
	float target_psi,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config);

void pascal_gov_cpu_update_integral_params(
	pascal_gov_load_state *PASCAL_GOV_RESTRICT state, float bat_level,
	float dt_safe,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	float *PASCAL_GOV_RESTRICT out_integral_total,
	float *PASCAL_GOV_RESTRICT out_integral_dot);

float pascal_gov_cpu_calculate_load_demand(
	pascal_gov_load_state *PASCAL_GOV_RESTRICT state,
	const pascal_gov_demand_input *PASCAL_GOV_RESTRICT input,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config);

__attribute__((pure)) float pascal_gov_cpu_calculate_trend_gain(float velocity);

__attribute__((pure)) float pascal_gov_cpu_calculate_effective_pressure(
	float load_demand, float trend_factor,
	const pascal_gov_cpu_math_config *math_config);

__attribute__((pure)) float pascal_gov_cpu_calculate_thermal_latency_limit(
	float thermal_scale, const pascal_gov_cpu_limits *kernel_limits);

void pascal_gov_cpu_calculate_latency_and_granularity(
	float p_eff, float load_demand, float thermal_min_latency_ns,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits,
	float *PASCAL_GOV_RESTRICT out_latency,
	float *PASCAL_GOV_RESTRICT out_granularity);

__attribute__((pure)) float pascal_gov_cpu_calculate_wakeup_granularity(
	float p_eff,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits);

__attribute__((pure)) float pascal_gov_cpu_calculate_migration_cost(
	float velocity, float p_eff,
	const pascal_gov_cpu_limits *kernel_limits);

__attribute__((pure)) float
pascal_gov_cpu_calculate_walt_init(float pressure,
				   const pascal_gov_cpu_limits *kernel_limits);

__attribute__((pure)) float pascal_gov_cpu_calculate_uclamp_min(
	float pressure, float thermal_scale,
	const pascal_gov_cpu_math_config *PASCAL_GOV_RESTRICT math_config,
	const pascal_gov_cpu_limits *PASCAL_GOV_RESTRICT kernel_limits);

#endif // PASCAL_GOV_CPU_H
