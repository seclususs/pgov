// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_KALMAN_H
#define PG_KALMAN_H

#include "compiler.h"
#include "pg/math.h"
#include <stdbool.h>

/**
 * @brief State container for the highly-optimized, fixed-point Kalman Filter.
 *
 * @note MATHEMATICAL CONTRACTS:
 *       - Operates entirely on Q16/Q32 fixed-point arithmetic to bypass FPU latency.
 *       - Tracks positional load (`x_pos`) and load velocity/derivative (`x_vel`).
 *       - Maintains a covariance matrix (`p00`, `p01`, etc.) to predict measurement noise.
 *       - Calculates `nis` (Normalized Innovation Squared) to aggressively break the 
 *         filter's inertia during sudden architectural workload shifts (e.g., app launches).
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

void pg_kalman_init(struct pg_kalman_state *RESTRICT state);

void pg_kalman_reset(struct pg_kalman_state *state);

/**
 * @brief Injects new raw measurements and advances the filter prediction by dt_sec.
 *
 * @param state  The filter state matrix. Borrowed for mutation.
 * @param z_meas The raw, noisy measurement metric (e.g., PSI total or CPU load).
 * @param dt_sec Delta time in Q16 fixed-point format since the last measurement.
 *
 * @return The smoothed, predicted state value (`x_pos`) after filtering the noise.
 */
q16_t pg_kalman_update(struct pg_kalman_state *state, q16_t z_meas,
		       q16_t dt_sec);

#endif // PG_KALMAN_H
