// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/config.h"
#include "pg/math.h"

struct pg_cpu_lim LIM_CPU = { .min_lat = FLOAT_TO_Q16(8.0F),
			      .max_lat = FLOAT_TO_Q16(20.0F),
			      .min_gran = FLOAT_TO_Q16(2.5F),
			      .max_gran = FLOAT_TO_Q16(6.5F),
			      .min_wake = FLOAT_TO_Q16(1.5F),
			      .max_wake = FLOAT_TO_Q16(6.5F),
			      .min_mig = FLOAT_TO_Q16(0.2F),
			      .max_mig = FLOAT_TO_Q16(0.6F),
			      .min_walt = FLOAT_TO_Q16(10.0F),
			      .max_walt = FLOAT_TO_Q16(40.0F),
			      .min_ucl = FLOAT_TO_Q16(0.0F),
			      .max_ucl = FLOAT_TO_Q16(384.0F) };

const struct pg_cpu_cfg CFG_CPU = { .lat_gran_rat = FLOAT_TO_Q16(0.34F),
				    .uclamp_k = FLOAT_TO_Q16(0.185F),
				    .stab_rat = FLOAT_TO_Q16(2.18F),
				    .gain_alpha = FLOAT_TO_Q16(0.972F),
				    .sigmoid_k = FLOAT_TO_Q16(0.072F),
				    .surge_thresh = FLOAT_TO_Q16(17.5F),
				    .trans_rate = FLOAT_TO_Q16(0.115F),
				    .trans_diff = FLOAT_TO_Q16(0.58F),
				    .trans_poll = FLOAT_TO_Q16(52.0F),
				    .nis_thresh = FLOAT_TO_Q16(7.8F) };

const struct pg_thermal_cfg CFG_THERMAL = { .limit_cpu = FLOAT_TO_Q16(52.5F),
					    .limit_bat = FLOAT_TO_Q16(40.5F),
					    .kp_base = FLOAT_TO_Q16(0.61F),
					    .ki_base = FLOAT_TO_Q16(0.0095F),
					    .kd_base = FLOAT_TO_Q16(0.82F) };
