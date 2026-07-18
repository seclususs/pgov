// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/tune.h"
#include "pg/config.h"
#include "topo.h"
#include "detect.h"
#include "sensor.h"

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
	LIM_CPU.max_ucl =
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

void pg_tune_configs(void)
{
	int32_t khz;
	int32_t mhz;
	int32_t cap;
	int32_t min_c;
	int32_t max_c;
	int32_t hz;
	q16_t rat;
	q16_t rel;
	q16_t f_val;
	q16_t therm;
	q16_t val;
	q16_t trip;
	q16_t psi;
	q16_t noise;
	q16_t tick;

	cap = 0;
	min_c = 0;
	max_c = 0;

	bool eas = (pg_topo_get_cpu_capacity(&cap, &min_c, &max_c) == 0);
	khz = pg_topo_get_max_freq_khz();
	int32_t core = pg_topo_get_core_count();

	if (!eas || max_c <= 0 || khz <= 0) {
		q16_t c;
		q16_t f;
		q16_t r_core;
		q16_t r_frq;
		mhz = (khz > 0) ? (khz / 1000) : 2000;

		c = INT_TO_Q16((core > 0) ? core : 8);
		f = INT_TO_Q16(mhz);

		r_core = pg_math_clamp(q16_div(c, INT_TO_Q16(8)), 0, Q16_ONE);
		r_frq = pg_math_clamp(q16_div(f, INT_TO_Q16(3500)), 0, Q16_ONE);

		rel = q16_mul(r_core, FLOAT_TO_Q16(0.40F)) +
		      q16_mul(r_frq, FLOAT_TO_Q16(0.60F));
		rel = pg_math_clamp(rel, 0, Q16_ONE);

		rat = (core > 4) ? FLOAT_TO_Q16(0.50F) : Q16_ONE;
	} else {
		mhz = khz / 1000;

		rat = q16_div(INT_TO_Q16(min_c), INT_TO_Q16(max_c));
		rat = pg_math_clamp(rat, 0, Q16_ONE);

		rel = q16_div(INT_TO_Q16(cap), INT_TO_Q16(8192));
		rel = pg_math_clamp(rel, 0, Q16_ONE);
	}

	f_val = q16_div(INT_TO_Q16(mhz), INT_TO_Q16(3500));
	f_val = pg_math_clamp(f_val, 0, Q16_ONE);

	therm = q16_mul(rel, f_val);
	therm = pg_math_clamp(therm, 0, Q16_ONE);

	hz = pg_detect_kernel_hz();
	tick = (hz > 0) ? q16_div(INT_TO_Q16(1000), INT_TO_Q16(hz)) :
			  INT_TO_Q16(10);

	val = q16_mul(tick, INT_TO_Q16(5));
	CFG_CPU.trans_poll =
		pg_math_clamp(val, FLOAT_TO_Q16(20.0F), FLOAT_TO_Q16(100.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.50F), FLOAT_TO_Q16(0.20F), rel);
	CFG_CPU.lat_gran_rat =
		pg_math_clamp(val, FLOAT_TO_Q16(0.20F), FLOAT_TO_Q16(0.50F));

	psi = FLOAT_TO_Q16(2.0F);
	noise = q16_div(psi, FLOAT_TO_Q16(10.0F));
	noise = pg_math_clamp(noise, 0, Q16_ONE);

	val = pg_math_lerp(FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(12.0F), noise);
	CFG_CPU.nis_thresh =
		pg_math_clamp(val, FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(12.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(3.5F), noise);
	CFG_CPU.stab_rat =
		pg_math_clamp(val, FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(3.5F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.99F), FLOAT_TO_Q16(0.85F), noise);
	CFG_CPU.gain_alpha =
		pg_math_clamp(val, FLOAT_TO_Q16(0.85F), FLOAT_TO_Q16(0.99F));

	trip = pg_sensor_get_trip_temp(FLOAT_TO_Q16(57.5F));
	val = trip - INT_TO_Q16(5);
	CFG_THERMAL.limit_cpu =
		pg_math_clamp(val, FLOAT_TO_Q16(45.0F), FLOAT_TO_Q16(90.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(42.0F), FLOAT_TO_Q16(38.0F), therm);
	CFG_THERMAL.limit_bat =
		pg_math_clamp(val, FLOAT_TO_Q16(38.0F), FLOAT_TO_Q16(42.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F), therm);
	CFG_THERMAL.ki_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.12F), FLOAT_TO_Q16(0.25F), rat);
	CFG_CPU.uclamp_k =
		pg_math_clamp(val, FLOAT_TO_Q16(0.12F), FLOAT_TO_Q16(0.25F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.04F), FLOAT_TO_Q16(0.12F), rat);
	CFG_CPU.sigmoid_k =
		pg_math_clamp(val, FLOAT_TO_Q16(0.04F), FLOAT_TO_Q16(0.12F));

	val = pg_math_lerp(FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F), rel);
	CFG_CPU.surge_thresh =
		pg_math_clamp(val, FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.08F), FLOAT_TO_Q16(0.20F), rel);
	CFG_CPU.trans_rate =
		pg_math_clamp(val, FLOAT_TO_Q16(0.08F), FLOAT_TO_Q16(0.20F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.40F), FLOAT_TO_Q16(0.90F), rel);
	CFG_CPU.trans_diff =
		pg_math_clamp(val, FLOAT_TO_Q16(0.40F), FLOAT_TO_Q16(0.90F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F), therm);
	CFG_THERMAL.kp_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F), therm);
	CFG_THERMAL.kd_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F));
}
