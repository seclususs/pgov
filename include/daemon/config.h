// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_CONFIG_H
#define PASCAL_GOV_DAEMON_CONFIG_H

#include "pascal_gov/cpu.h"
#include "pascal_gov/filter.h"
#include "pascal_gov/poller.h"
#include "pascal_gov/thermal.h"

#define PASCAL_GOV_MIN_POLLING_MS 100
#define PASCAL_GOV_MAX_POLLING_MS 10000
#define PASCAL_GOV_MAX_EVENTS 16
#define PASCAL_GOV_STABILIZATION_DELAY_SEC 60
#define PASCAL_GOV_BOOT_WAIT_RETRY_LIMIT 300
#define PASCAL_GOV_BOOT_POLL_INTERVAL_SEC 1

static const pascal_gov_cpu_limits PASCAL_GOV_CONFIG_CPU_LIMITS = {
	.min_latency_ns = 8000000.0F,
	.max_latency_ns = 20000000.0F,
	.min_granularity_ns = 2500000.0F,
	.max_granularity_ns = 6500000.0F,
	.min_wakeup_ns = 1500000.0F,
	.max_wakeup_ns = 6500000.0F,
	.min_migration_cost = 200000.0F,
	.max_migration_cost = 600000.0F,
	.min_walt_init_pct = 10.0F,
	.max_walt_init_pct = 40.0F,
	.min_uclamp_min = 0.0F,
	.max_uclamp_min = 384.0F};

static const pascal_gov_cpu_math_config PASCAL_GOV_CONFIG_CPU_MATH = {
	.latency_gran_ratio = 0.34F,
	.decay_coeff = 0.019F,
	.uclamp_k = 0.185F,
	.uclamp_mid = 9.5F,
	.response_gain = 31.0F,
	.stability_ratio = 2.18F,
	.stability_margin = 3.1F,
	.gain_scheduling_alpha = 0.972F,
	.sigmoid_k = 0.072F,
	.sigmoid_mid = 6.8F,
	.lookahead_time = 0.17F,
	.trend_amplification = 0.115F,
	.surge_threshold = 17.5F,
	.surge_gain = 0.095F,
	.transient_rate_threshold = 0.115F,
	.transient_diff_threshold = 0.58F,
	.transient_poll_interval = 52.0F,
	.nis_threshold = 7.8F,
	.bat_level_weight = 97.0F};

static const pascal_gov_controller_config PASCAL_GOV_CONFIG_CONTROLLER = {
	.poll_weight_pressure = 1.5F,
	.poll_weight_derivative = 0.05F,
	.battery_check_interval_sec = 5,
	.psi_threshold_us = 100000,
	.psi_window_us = 1000000};

static const pascal_gov_kalman_config PASCAL_GOV_CONFIG_KALMAN = {
	.q_pos = 0.08F, .q_vel = 8.0F, .r_meas = 4.0F, .fading_factor = 1.02F};

static const pascal_gov_poller_config PASCAL_GOV_CONFIG_POLLER = {
	.sleep_tolerance_ms = 200,
	.quantization_step_ms = 50,
	.hysteresis_threshold_ms = 200,
	.noise_percent = 5,
	.rise_factor = 0.03F,
	.fall_factor = 0.95F};

static const pascal_gov_thermal_config PASCAL_GOV_CONFIG_THERMAL = {
	.hard_limit_cpu = 52.5F,
	.hard_limit_bat = 40.5F,
	.sched_temp_cool = 25.5F,
	.sched_temp_hot = 45.5F,
	.kp_base = 0.61F,
	.ki_base = 0.0095F,
	.kd_base = 0.82F,
	.kp_fast = 4.05F,
	.ki_fast = 0.059F,
	.kd_fast = 3.55F,
	.anti_windup_k = 1.95F,
	.deriv_filter_n = 18.5F,
	.ff_gain = 2.55F,
	.ff_lead_time = 4.8F,
	.ff_lag_time = 2.3F,
	.smith_gain = 1.52F,
	.smith_tau = 11.5F,
	.smith_delay_sec = 1.7F};

#endif // PASCAL_GOV_DAEMON_CONFIG_H
