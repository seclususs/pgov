// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include "pg/cpu.h"
#include "pg/thermal.h"
#include "pg/math.h"

#define PG_MIN_POLL_MS 100
#define PG_MAX_POLL_MS 10000
#define PG_MAX_EVENTS 16
#define PG_STAB_DELAY_SEC 10
#define PG_BOOT_RETRY_MAX 300
#define PG_BOOT_POLL_SEC 1
#define PG_QUANT_NS 65536ULL
#define PG_CHK_LAT 100
#define PG_CHK_GRAN 100
#define PG_CHK_WAKE 150
#define PG_CHK_MIG 50000
#define PG_CHK_WALT 5
#define PG_CHK_UCL 32
#define PG_RT_PRIORITY 50
#define PG_UCLAMP_MAX 102
#define PG_STACK_SIZE (512UL * 1024UL)
#define PG_TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define PG_IOPRIO_CLASS 2
#define PG_IOPRIO_DATA 0
#define PG_BAT_CHK_SEC 5
#define PG_PSI_THRESHOLD_US 100000
#define PG_PSI_WINDOW_US 1000000

#if defined(NDK_BUILD)
#define PG_SWEEP_IVL_SEC 600
#endif

extern struct pg_cpu_lim LIM_CPU;

static const struct pg_cpu_cfg PG_CFG_CPU = {
	.lat_gran_rat = FLOAT_TO_Q16(0.34F),
	.decay = FLOAT_TO_Q16(0.019F),
	.uclamp_k = FLOAT_TO_Q16(0.185F),
	.uclamp_mid = FLOAT_TO_Q16(9.5F),
	.resp_gain = FLOAT_TO_Q16(31.0F),
	.stab_rat = FLOAT_TO_Q16(2.18F),
	.stab_marg = FLOAT_TO_Q16(3.1F),
	.gain_alpha = FLOAT_TO_Q16(0.972F),
	.sigmoid_k = FLOAT_TO_Q16(0.072F),
	.sigmoid_mid = FLOAT_TO_Q16(6.8F),
	.lookahead = FLOAT_TO_Q16(0.17F),
	.trend_amp = FLOAT_TO_Q16(0.115F),
	.surge_thresh = FLOAT_TO_Q16(17.5F),
	.surge_gain = FLOAT_TO_Q16(0.095F),
	.trans_rate = FLOAT_TO_Q16(0.115F),
	.trans_diff = FLOAT_TO_Q16(0.58F),
	.trans_poll = FLOAT_TO_Q16(52.0F),
	.nis_thresh = FLOAT_TO_Q16(7.8F),
	.bat_wt = FLOAT_TO_Q16(97.0F)
};

static const struct pg_thermal_cfg PG_CFG_THERMAL = {
	.limit_cpu = FLOAT_TO_Q16(52.5F),
	.limit_bat = FLOAT_TO_Q16(40.5F),
	.temp_cool = FLOAT_TO_Q16(25.5F),
	.temp_hot = FLOAT_TO_Q16(45.5F),
	.kp_base = FLOAT_TO_Q16(0.61F),
	.ki_base = FLOAT_TO_Q16(0.0095F),
	.kd_base = FLOAT_TO_Q16(0.82F),
	.kp_fast = FLOAT_TO_Q16(4.05F),
	.ki_fast = FLOAT_TO_Q16(0.059F),
	.kd_fast = FLOAT_TO_Q16(3.55F),
	.aw_k = FLOAT_TO_Q16(1.95F),
};

#endif // PG_CONFIG_H
