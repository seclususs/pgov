// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/tune.h"
#include "pg/config.h"
#include "topo.h"
#include "sensor.h"

static inline q16_t calc_heuristic_cap(int32_t core, int32_t mhz)
{
	q16_t c_val = INT_TO_Q16((core > 0) ? core : 8);
	q16_t f_val = INT_TO_Q16((mhz > 0) ? mhz : 2000);

	q16_t r_core = pg_math_clamp(q16_div(c_val, INT_TO_Q16(8)), 0, Q16_ONE);
	q16_t r_freq =
		pg_math_clamp(q16_div(f_val, INT_TO_Q16(3500)), 0, Q16_ONE);

	q16_t cap = q16_mul(r_core, FLOAT_TO_Q16(0.40F)) +
		    q16_mul(r_freq, FLOAT_TO_Q16(0.60F));

	return pg_math_clamp(cap, 0, Q16_ONE);
}

void pg_tune_limits(void)
{
	int32_t core = pg_topo_get_core_count();
	int32_t freq_khz = pg_topo_get_max_freq_khz();
	if (core <= 0 || freq_khz <= 0)
		return;

	q16_t cap = calc_heuristic_cap(core, freq_khz / 1000);
	q16_t val;

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

static void calc_topo_metrics(q16_t *RESTRICT rat, q16_t *RESTRICT rel,
			      q16_t *RESTRICT therm)
{
	int32_t cap = 0;
	int32_t min_c = 0;
	int32_t max_c = 0;
	int32_t mhz;

	bool eas = (pg_topo_get_cpu_capacity(&cap, &min_c, &max_c) == 0);
	int32_t khz = pg_topo_get_max_freq_khz();
	int32_t core = pg_topo_get_core_count();

	if (!eas || max_c <= 0 || khz <= 0) {
		mhz = (khz > 0) ? (khz / 1000) : 2000;
		*rel = calc_heuristic_cap(core, mhz);
		*rat = (core > 4) ? FLOAT_TO_Q16(0.50F) : Q16_ONE;
	} else {
		mhz = khz / 1000;

		*rat = q16_div(INT_TO_Q16(min_c), INT_TO_Q16(max_c));
		*rat = pg_math_clamp(*rat, 0, Q16_ONE);

		*rel = q16_div(INT_TO_Q16(cap), INT_TO_Q16(8192));
		*rel = pg_math_clamp(*rel, 0, Q16_ONE);
	}

	q16_t f_val = q16_div(INT_TO_Q16(mhz), INT_TO_Q16(3500));
	f_val = pg_math_clamp(f_val, 0, Q16_ONE);

	*therm = q16_mul(*rel, f_val);
	*therm = pg_math_clamp(*therm, 0, Q16_ONE);
}

static void tune_cpu_configs(q16_t rel, q16_t rat)
{
	q16_t val;

	val = pg_math_lerp(FLOAT_TO_Q16(0.50F), FLOAT_TO_Q16(0.20F), rel);
	CFG_CPU.lat_gran_rat =
		pg_math_clamp(val, FLOAT_TO_Q16(0.20F), FLOAT_TO_Q16(0.50F));

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
}

static void tune_thermal_configs(q16_t therm)
{
	q16_t trip = pg_sensor_get_trip_temp(FLOAT_TO_Q16(57.5F));
	q16_t val = trip - INT_TO_Q16(5);
	CFG_THERMAL.limit_cpu =
		pg_math_clamp(val, FLOAT_TO_Q16(45.0F), FLOAT_TO_Q16(90.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(42.0F), FLOAT_TO_Q16(38.0F), therm);
	CFG_THERMAL.limit_bat =
		pg_math_clamp(val, FLOAT_TO_Q16(38.0F), FLOAT_TO_Q16(42.0F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F), therm);
	CFG_THERMAL.ki_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F), therm);
	CFG_THERMAL.kp_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F));

	val = pg_math_lerp(FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F), therm);
	CFG_THERMAL.kd_base =
		pg_math_clamp(val, FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F));
}

void pg_tune_configs(void)
{
	q16_t rat = 0;
	q16_t rel = 0;
	q16_t therm = 0;

	calc_topo_metrics(&rat, &rel, &therm);
	tune_cpu_configs(rel, rat);
	tune_thermal_configs(therm);
}
