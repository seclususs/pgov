// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include "pg/cpu.h"
#include "pg/kalman.h"
#include "pg/poll.h"
#include "pg/thermal.h"

#define PG_MIN_POLL_MS 100
#define PG_MAX_POLL_MS 10000
#define PG_MAX_EVENTS 16
#define PG_STAB_DELAY_SEC 10
#define PG_BOOT_RETRY_MAX 300
#define PG_BOOT_POLL_SEC 1
#define PG_SYSFS_QUANT_NS 65536ULL
#define PG_CHK_LAT 100
#define PG_CHK_GRAN 100
#define PG_CHK_WAKEUP 150
#define PG_CHK_MIG 50000
#define PG_CHK_WALT 5
#define PG_CHK_UCLAMP 32
#define PG_RT_PRIORITY 50
#define PG_UCLAMP_MAX 102
#define PG_STACK_SIZE (512UL * 1024UL)
#define PG_TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define PG_IOPRIO_CLASS 2
#define PG_IOPRIO_DATA 0

static const struct pg_cpu_lim PG_LIM_CPU = { .min_lat = 8000000.0F,
					      .max_lat = 20000000.0F,
					      .min_gran = 2500000.0F,
					      .max_gran = 6500000.0F,
					      .min_wake = 1500000.0F,
					      .max_wake = 6500000.0F,
					      .min_mig = 200000.0F,
					      .max_mig = 600000.0F,
					      .min_walt = 10.0F,
					      .max_walt = 40.0F,
					      .min_uclamp = 0.0F,
					      .max_uclamp = 384.0F };

static const struct pg_cpu_cfg PG_CFG_CPU = { .lat_gran_rat = 0.34F,
					      .decay = 0.019F,
					      .uclamp_k = 0.185F,
					      .uclamp_mid = 9.5F,
					      .resp_gain = 31.0F,
					      .stab_rat = 2.18F,
					      .stab_marg = 3.1F,
					      .gain_alpha = 0.972F,
					      .sigmoid_k = 0.072F,
					      .sigmoid_mid = 6.8F,
					      .lookahead = 0.17F,
					      .trend_amp = 0.115F,
					      .surge_thresh = 17.5F,
					      .surge_gain = 0.095F,
					      .trans_rate = 0.115F,
					      .trans_diff = 0.58F,
					      .trans_poll = 52.0F,
					      .nis_thresh = 7.8F,
					      .bat_wt = 97.0F };

static const struct pg_ctrl_cfg PG_CFG_CTRL = { .press_wt = 1.5F,
						.deriv_wt = 0.05F,
						.bat_chk_sec = 5,
						.thresh_us = 100000,
						.win_us = 1000000 };

static const struct pg_kalman_cfg PG_CFG_KALMAN = { .q_pos = 0.08F,
						    .q_vel = 8.0F,
						    .r_meas = 4.0F,
						    .fade_fact = 1.02F };

static const struct pg_poll_cfg PG_CFG_POLL = { .sleep_tol = 200,
						.quant = 50,
						.hyst = 200,
						.noise_pct = 5,
						.rise_fact = 0.03F,
						.fall_fact = 0.95F };

static const struct pg_thermal_cfg PG_CFG_THERMAL = { .limit_cpu = 52.5F,
						      .limit_bat = 40.5F,
						      .temp_cool = 25.5F,
						      .temp_hot = 45.5F,
						      .kp_base = 0.61F,
						      .ki_base = 0.0095F,
						      .kd_base = 0.82F,
						      .kp_fast = 4.05F,
						      .ki_fast = 0.059F,
						      .kd_fast = 3.55F,
						      .aw_k = 1.95F,
						      .filt_n = 18.5F,
						      .ff_gain = 2.55F,
						      .ff_lead_t = 4.8F,
						      .ff_lag_t = 2.3F,
						      .smith_gain = 1.52F,
						      .smith_tau = 11.5F,
						      .smith_delay = 1.7F };

#endif // PG_CONFIG_H
