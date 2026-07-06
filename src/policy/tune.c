// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/tune.h"
#include "pg/config.h"
#include "topo.h"

void pg_tune_limits(void)
{
	int32_t core;
	int32_t freq_khz;
	int32_t freq_mhz;
	q16_t c_val;
	q16_t f_val;
	q16_t r_core;
	q16_t r_freq;
	q16_t w_core;
	q16_t w_freq;
	q16_t cap;
	q16_t val;

	core = pg_topo_get_core_count();
	freq_khz = pg_topo_get_max_freq_khz();
	if (core <= 0 || freq_khz <= 0)
		return;

	freq_mhz = freq_khz / 1000;
	c_val = INT_TO_Q16(core);
	f_val = INT_TO_Q16(freq_mhz);

	r_core = pg_math_clamp(q16_div(c_val, INT_TO_Q16(8)), 0, Q16_ONE);
	r_freq = pg_math_clamp(q16_div(f_val, INT_TO_Q16(3500)), 0, Q16_ONE);
	w_core = FLOAT_TO_Q16(0.40F);
	w_freq = FLOAT_TO_Q16(0.60F);

	cap = q16_mul(r_core, w_core) + q16_mul(r_freq, w_freq);
	cap = pg_math_clamp(cap, 0, Q16_ONE);

	val = pg_math_lerp(FLOAT_TO_Q16(384.0F), FLOAT_TO_Q16(1024.0F), cap);
	LIM_CPU.max_uclamp =
		pg_math_clamp(val, FLOAT_TO_Q16(384.0F), FLOAT_TO_Q16(1024.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(20.0F), FLOAT_TO_Q16(4.0F), cap);
	LIM_CPU.min_lat =
		pg_math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(20.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(25.0F), FLOAT_TO_Q16(12.0F), cap);
	LIM_CPU.max_lat =
		pg_math_clamp(val, FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(6.5F), FLOAT_TO_Q16(1.5F), cap);
	LIM_CPU.min_gran =
		pg_math_clamp(val, FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(6.5F));

	val = pg_math_lerp(FLOAT_TO_Q16(10.0F), FLOAT_TO_Q16(4.0F), cap);
	LIM_CPU.max_gran =
		pg_math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(10.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(1.0F), cap);
	LIM_CPU.min_wake =
		pg_math_clamp(val, FLOAT_TO_Q16(1.0F), FLOAT_TO_Q16(3.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(8.0F), FLOAT_TO_Q16(4.0F), cap);
	LIM_CPU.max_wake =
		pg_math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(8.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.6F), FLOAT_TO_Q16(0.2F), cap);
	LIM_CPU.min_mig =
		pg_math_clamp(val, FLOAT_TO_Q16(0.2F), FLOAT_TO_Q16(0.6F));

	val = pg_math_lerp(FLOAT_TO_Q16(1.2F), FLOAT_TO_Q16(0.6F), cap);
	LIM_CPU.max_mig =
		pg_math_clamp(val, FLOAT_TO_Q16(0.6F), FLOAT_TO_Q16(1.2F));
}
