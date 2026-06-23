// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_MATH_H
#define PASCAL_GOV_MATH_H

#include "pascal_gov/compiler.h"
#include <math.h>
#include <stdint.h>

static inline __attribute__((always_inline, const)) float
pascal_gov_clampf(float val, float min, float max)
{
	return fmaxf(min, fminf(val, max));
}

static inline __attribute__((always_inline, const)) float
pascal_gov_sigmoid_param(float val, float k, float mid)
{
	float x = k * (val - mid);
	return 0.5F * ((x / (1.0F + fabsf(x))) + 1.0F);
}

static inline __attribute__((always_inline, const)) float
pascal_gov_decay(float val, float coeff)
{
	float x = coeff * val;
	if (PASCAL_GOV_UNLIKELY(x < 0.0F))
		return 1.0F;

	return 1.0F / (1.0F + x + (0.5F * x * x));
}

static inline __attribute__((always_inline, const)) float
pascal_gov_tanh_approx(float x)
{
	return x / sqrtf(1.0F + (x * x));
}

static inline __attribute__((always_inline, const)) uint64_t
pascal_gov_sanitize_to_u64(float val, uint64_t fallback)
{
	if (PASCAL_GOV_UNLIKELY(!__builtin_isfinite(val) || val < 0.0F))
		return fallback;

	return (uint64_t)(val + 0.5F);
}

static inline __attribute__((always_inline, const)) uint64_t
pascal_gov_sanitize_to_clean_u64(float val, uint64_t fallback, uint64_t step)
{
	uint64_t val_u64 = pascal_gov_sanitize_to_u64(val, fallback);
	if (PASCAL_GOV_UNLIKELY(step == 0 || step == 1))
		return val_u64;

	uint64_t remainder = val_u64 % step;
	if (remainder >= step / 2)
		return val_u64 + step - remainder;

	return val_u64 - remainder;
}

#endif // PASCAL_GOV_MATH_H
