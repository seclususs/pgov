// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/filter.h"
#include "pascal_gov/math.h"
#include <math.h>

void pascal_gov_kalman_init(
	pascal_gov_kalman_state *PASCAL_GOV_RESTRICT state,
	const pascal_gov_kalman_config *PASCAL_GOV_RESTRICT config)
{
	state->config = *config;
	pascal_gov_kalman_reset(state);
}

void pascal_gov_kalman_reset(pascal_gov_kalman_state *PASCAL_GOV_RESTRICT state)
{
	state->first_run = true;
	state->x_pos = 0.0F;
	state->x_vel = 0.0F;
	state->p00 = 100.0F;
	state->p01 = 0.0F;
	state->p10 = 0.0F;
	state->p11 = 100.0F;
	state->last_nis = 0.0F;
}

float pascal_gov_kalman_update(
	pascal_gov_kalman_state *PASCAL_GOV_RESTRICT state, float z_meas,
	float dt_sec)
{
	if (PASCAL_GOV_UNLIKELY(!__builtin_isfinite(z_meas)))
		return state->x_pos;

	float z = pascal_gov_clampf(z_meas, 0.0F, 500.0F);

	if (PASCAL_GOV_UNLIKELY(dt_sec > 5.0F))
		pascal_gov_kalman_reset(state);

	if (PASCAL_GOV_UNLIKELY(state->first_run)) {
		state->x_pos = z;
		state->x_vel = 0.0F;
		state->first_run = false;
		return z;
	}

	float dt = fmaxf(dt_sec, 0.0001F);
	float dt2 = dt * dt;
	float dt3 = dt2 * dt;
	float dt4 = dt2 * dt2;

	float x_pos_pred = state->x_pos + state->x_vel * dt;
	float x_vel_pred = state->x_vel;

	float q_scale = state->config.q_vel;
	float q00 = q_scale * dt4 * 0.25F + state->config.q_pos * dt;
	float q01 = q_scale * dt3 * 0.5F;
	float q10 = q01;
	float q11 = q_scale * dt2 + state->config.q_vel * dt;

	float f_p00 = state->p00 + state->p10 * dt;
	float f_p01 = state->p01 + state->p11 * dt;
	float f_p10 = state->p10;
	float f_p11 = state->p11;

	float alpha = state->config.fading_factor;
	float p00_pred = (f_p00 + f_p01 * dt) * alpha + q00;
	float p01_pred = f_p01 * alpha + q01;
	float p10_pred = (f_p10 + f_p11 * dt) * alpha + q10;
	float p11_pred = f_p11 * alpha + q11;

	float y = z - x_pos_pred;
	float s = p00_pred + state->config.r_meas;

	float inv_s = (fabsf(s) > 1e-9F) ? (1.0F / s) : 0.0F;

	float k0 = p00_pred * inv_s;
	float k1 = p10_pred * inv_s;

	state->x_pos = x_pos_pred + k0 * y;
	state->x_vel = x_vel_pred + k1 * y;

	state->p00 = (1.0F - k0) * p00_pred;
	state->p01 = (1.0F - k0) * p01_pred;
	state->p10 = -k1 * p00_pred + p10_pred;
	state->p11 = -k1 * p01_pred + p11_pred;

	state->last_nis = y * y * inv_s;

	return fmaxf(state->x_pos, 0.0F);
}
