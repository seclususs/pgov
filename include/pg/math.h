// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_MATH_H
#define PG_MATH_H

#include "compiler.h"
#include <math.h>
#include <stdint.h>

static ALWAYS_INLINE CONST float pg_math_clampf(float val, float min, float max)
{
	return fmaxf(min, fminf(val, max));
}

static ALWAYS_INLINE CONST float pg_math_sigmoid(float val, float k, float mid)
{
	float x = k * (val - mid);
	return 0.5F * ((x / (1.0F + fabsf(x))) + 1.0F);
}

static ALWAYS_INLINE CONST float pg_math_decay(float val, float coeff)
{
	float x = coeff * val;
	if (UNLIKELY(x < 0.0F))
		return 1.0F;

	return 1.0F / (1.0F + x + (0.5F * x * x));
}

static ALWAYS_INLINE CONST float pg_math_tanh_approx(float x)
{
	return x / sqrtf(1.0F + (x * x));
}

static ALWAYS_INLINE CONST uint64_t pg_math_san_u64(float val,
						    uint64_t fallback)
{
	if (UNLIKELY(!__builtin_isfinite(val) || val < 0.0F))
		return fallback;

	return (uint64_t)(val + 0.5F);
}

static ALWAYS_INLINE CONST uint64_t pg_math_san_clean_u64(float val,
							  uint64_t fallback,
							  uint64_t step)
{
	uint64_t val_u64 = pg_math_san_u64(val, fallback);
	if (UNLIKELY(step == 0 || step == 1))
		return val_u64;

	uint64_t remainder = val_u64 % step;
	if (remainder >= step / 2)
		return val_u64 + step - remainder;

	return val_u64 - remainder;
}

#endif // PG_MATH_H
