// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_KALMAN_H
#define PG_KALMAN_H

#include "pg/math.h"
#include <stdbool.h>

/**
 * @brief State container for the highly-optimized, fixed-point Kalman Filter.
 *
 * @note MATHEMATICAL CONTRACTS:
 *       - Operates entirely on Q16/Q32 fixed-point arithmetic to bypass FPU latency.
 *       - Tracks the filtered signal (`x_pos`) and its velocity (`x_vel`):
 *         a PSI stall percentage for the PSI monitor, a CPU temperature in
 *         degrees Celsius for the thermal loop.
 *       - Maintains the state covariance matrix (`p00`, `p01`, etc.) and adapts the
 *         measurement and process noise (`r_meas`, `q_vel`) from an EMA of the
 *         innovation covariance (`cov_y`).
 *       - Calculates `nis` (Normalized Innovation Squared) from the raw innovation so
 *         that consumers can detect a structural break during sudden architectural
 *         workload shifts (e.g., app launches) and snap their own state to the
 *         measurement.
 */
struct pg_kalman_state {
	q16_t x_pos;
	q16_t x_vel;
	q32_t p00;
	q32_t p01;
	q32_t p10;
	q32_t p11;
	q16_t q_vel;
	q16_t r_meas;
	q32_t cov_y;
	q16_t nis;
	bool first_run;
};

void pg_kalman_init(struct pg_kalman_state *state);

void pg_kalman_reset(struct pg_kalman_state *state);

/**
 * @brief Injects new raw measurements and advances the filter prediction by dt_sec.
 *
 * @param state  The filter state matrix. Borrowed for mutation.
 * @param z_meas The raw, noisy measurement (e.g., PSI stall percentage or CPU
 *               temperature in degrees Celsius), clamped to [0, 500].
 * @param dt_sec Delta time in Q16 fixed-point format since the last measurement.
 *
 * @return The smoothed position estimate (`x_pos`) after the measurement update,
 *         floored at zero.
 */
q16_t pg_kalman_update(struct pg_kalman_state *state, q16_t z_meas,
		       q16_t dt_sec);

#endif // PG_KALMAN_H
