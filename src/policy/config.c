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
