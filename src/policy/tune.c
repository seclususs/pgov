// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/tune.h"
#include "pgov/param.h"
#include "pgov/topo.h"
#include "pgov/sensor.h"

static inline q16_t calc_heuristic_cap(int32_t core, int32_t mhz)
{
	q16_t c_val = INT_TO_Q16((core > 0) ? core : 8);
	q16_t f_val = INT_TO_Q16((mhz > 0) ? mhz : 2000);

	q16_t r_core =
		math_clamp(math_div_q16(c_val, INT_TO_Q16(8)), 0, Q16_ONE);
	q16_t r_freq =
		math_clamp(math_div_q16(f_val, INT_TO_Q16(3500)), 0, Q16_ONE);

	q16_t cap = math_mul_q16(r_core, FLOAT_TO_Q16(0.40F)) +
		    math_mul_q16(r_freq, FLOAT_TO_Q16(0.60F));

	return math_clamp(cap, 0, Q16_ONE);
}

void pg_tune_limits(void)
{
	int32_t core = topo_get_core_count();
	int32_t freq_khz = topo_get_maxfrq_khz();
	if (core <= 0 || freq_khz <= 0)
		return;

	q16_t cap = calc_heuristic_cap(core, freq_khz / 1000);
	q16_t val;

	val = math_lerp(FLOAT_TO_Q16(384.0F), FLOAT_TO_Q16(1024.0F), cap);
	param_sched.max_ucl =
		math_clamp(val, FLOAT_TO_Q16(384.0F), FLOAT_TO_Q16(1024.0F));

	val = math_lerp(FLOAT_TO_Q16(448.0F), FLOAT_TO_Q16(640.0F), cap);
	param_sched.ucl_max =
		math_clamp(val, FLOAT_TO_Q16(448.0F), FLOAT_TO_Q16(640.0F));

	val = math_lerp(FLOAT_TO_Q16(320.0F), FLOAT_TO_Q16(448.0F), cap);
	param_sched.susp_ucl =
		math_clamp(val, FLOAT_TO_Q16(320.0F), FLOAT_TO_Q16(448.0F));

	val = math_lerp(FLOAT_TO_Q16(20.0F), FLOAT_TO_Q16(4.0F), cap);
	param_sched.min_lat =
		math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(20.0F));

	val = math_lerp(FLOAT_TO_Q16(25.0F), FLOAT_TO_Q16(12.0F), cap);
	param_sched.max_lat =
		math_clamp(val, FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F));

	val = math_lerp(FLOAT_TO_Q16(6.5F), FLOAT_TO_Q16(1.5F), cap);
	param_sched.min_gran =
		math_clamp(val, FLOAT_TO_Q16(1.5F), FLOAT_TO_Q16(6.5F));

	val = math_lerp(FLOAT_TO_Q16(10.0F), FLOAT_TO_Q16(4.0F), cap);
	param_sched.max_gran =
		math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(10.0F));

	val = math_lerp(FLOAT_TO_Q16(3.0F), FLOAT_TO_Q16(1.0F), cap);
	param_sched.min_wake =
		math_clamp(val, FLOAT_TO_Q16(1.0F), FLOAT_TO_Q16(3.0F));

	val = math_lerp(FLOAT_TO_Q16(8.0F), FLOAT_TO_Q16(4.0F), cap);
	param_sched.max_wake =
		math_clamp(val, FLOAT_TO_Q16(4.0F), FLOAT_TO_Q16(8.0F));

	val = math_lerp(FLOAT_TO_Q16(0.6F), FLOAT_TO_Q16(0.2F), cap);
	param_sched.min_mig =
		math_clamp(val, FLOAT_TO_Q16(0.2F), FLOAT_TO_Q16(0.6F));

	val = math_lerp(FLOAT_TO_Q16(1.2F), FLOAT_TO_Q16(0.6F), cap);
	param_sched.max_mig =
		math_clamp(val, FLOAT_TO_Q16(0.6F), FLOAT_TO_Q16(1.2F));
}

static void calc_topo_metrics(q16_t *RESTRICT rat, q16_t *RESTRICT rel,
			      q16_t *RESTRICT therm)
{
	int32_t cap = 0;
	int32_t min_c = 0;
	int32_t max_c = 0;
	int32_t mhz;

	bool eas = (topo_get_cpu_capacity(&cap, &min_c, &max_c) == 0);
	int32_t khz = topo_get_maxfrq_khz();
	int32_t core = topo_get_core_count();

	if (!eas || max_c <= 0 || khz <= 0) {
		mhz = (khz > 0) ? (khz / 1000) : 2000;
		*rel = calc_heuristic_cap(core, mhz);
		*rat = (core > 4) ? FLOAT_TO_Q16(0.50F) : Q16_ONE;
	} else {
		mhz = khz / 1000;

		*rat = math_div_q16(INT_TO_Q16(min_c), INT_TO_Q16(max_c));
		*rat = math_clamp(*rat, 0, Q16_ONE);

		*rel = math_div_q16(INT_TO_Q16(cap), INT_TO_Q16(8192));
		*rel = math_clamp(*rel, 0, Q16_ONE);
	}

	q16_t f_val = math_div_q16(INT_TO_Q16(mhz), INT_TO_Q16(3500));
	f_val = math_clamp(f_val, 0, Q16_ONE);

	*therm = math_mul_q16(*rel, f_val);
	*therm = math_clamp(*therm, 0, Q16_ONE);
}

static void tune_cpu_params(q16_t rel, q16_t rat)
{
	q16_t val;

	val = math_lerp(FLOAT_TO_Q16(0.50F), FLOAT_TO_Q16(0.20F), rel);
	param_cpu.lat_gran_rat =
		math_clamp(val, FLOAT_TO_Q16(0.20F), FLOAT_TO_Q16(0.50F));

	val = math_lerp(FLOAT_TO_Q16(0.12F), FLOAT_TO_Q16(0.25F), rat);
	param_cpu.uclamp_k =
		math_clamp(val, FLOAT_TO_Q16(0.12F), FLOAT_TO_Q16(0.25F));

	val = math_lerp(FLOAT_TO_Q16(0.04F), FLOAT_TO_Q16(0.12F), rat);
	param_cpu.sigmoid_k =
		math_clamp(val, FLOAT_TO_Q16(0.04F), FLOAT_TO_Q16(0.12F));

	val = math_lerp(FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F), rel);
	param_cpu.surge_thresh =
		math_clamp(val, FLOAT_TO_Q16(12.0F), FLOAT_TO_Q16(25.0F));

	val = math_lerp(FLOAT_TO_Q16(0.08F), FLOAT_TO_Q16(0.20F), rel);
	param_cpu.trans_rate =
		math_clamp(val, FLOAT_TO_Q16(0.08F), FLOAT_TO_Q16(0.20F));

	val = math_lerp(FLOAT_TO_Q16(0.40F), FLOAT_TO_Q16(0.90F), rel);
	param_cpu.trans_diff =
		math_clamp(val, FLOAT_TO_Q16(0.40F), FLOAT_TO_Q16(0.90F));
}

static void tune_thermal_params(q16_t therm)
{
	q16_t trip = sensor_get_trip_temp(FLOAT_TO_Q16(57.5F));
	q16_t val = trip - INT_TO_Q16(5);
	param_thermal.limit_cpu =
		math_clamp(val, FLOAT_TO_Q16(45.0F), FLOAT_TO_Q16(90.0F));

	val = math_lerp(FLOAT_TO_Q16(42.0F), FLOAT_TO_Q16(38.0F), therm);
	param_thermal.limit_bat =
		math_clamp(val, FLOAT_TO_Q16(38.0F), FLOAT_TO_Q16(42.0F));

	val = math_lerp(FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F), therm);
	param_thermal.ki_base =
		math_clamp(val, FLOAT_TO_Q16(0.005F), FLOAT_TO_Q16(0.015F));

	val = math_lerp(FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F), therm);
	param_thermal.kp_base =
		math_clamp(val, FLOAT_TO_Q16(0.45F), FLOAT_TO_Q16(0.90F));

	val = math_lerp(FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F), therm);
	param_thermal.kd_base =
		math_clamp(val, FLOAT_TO_Q16(0.60F), FLOAT_TO_Q16(1.30F));
}

void pg_tune_configs(void)
{
	q16_t rat = 0;
	q16_t rel = 0;
	q16_t therm = 0;

	calc_topo_metrics(&rat, &rel, &therm);
	tune_cpu_params(rel, rat);
	tune_thermal_params(therm);
}
