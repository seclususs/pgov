// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_MATH_H
#define PG_MATH_H

#include "compiler.h"
#include <stdint.h>

typedef int32_t q16_t;
typedef int64_t q32_t;

#define Q16_SHIFT 16
#define Q16_ONE (1 << Q16_SHIFT)
#define Q16_HALF (1 << (Q16_SHIFT - 1))

#define FLOAT_TO_Q16(f) ((q16_t)((f) * 65536.0f))
#define INT_TO_Q16(i) ((q16_t)((i) * Q16_ONE))
#define Q16_TO_INT(q) ((q) >> Q16_SHIFT)
#define Q16_TO_FLOAT(q) ((float)(q) / 65536.0f)

#define Q16_TO_Q32(q) ((q32_t)(q) * Q16_ONE)
#define Q32_TO_Q16(q) ((q16_t)((q) >> 16))

#define ABS_Q16(x)                      \
	({                              \
		__typeof__(x) _x = (x); \
		(_x < 0) ? -_x : _x;    \
	})

static ALWAYS_INLINE q16_t q16_mul(q16_t a, q16_t b)
{
	return (q16_t)(((q32_t)a * b) >> Q16_SHIFT);
}

static ALWAYS_INLINE q16_t q16_div(q16_t a, q16_t b)
{
	if (UNLIKELY(b == 0))
		return (a >= 0) ? INT32_MAX : INT32_MIN;

	return (q16_t)((((q32_t)a) << Q16_SHIFT) / b);
}

#if defined(__SIZEOF_INT128__)
static ALWAYS_INLINE q32_t q32_mul(q32_t a, q32_t b)
{
	return (q32_t)(((unsigned __int128)a * (unsigned __int128)b) >> 32);
}
#else
static ALWAYS_INLINE q32_t q32_mul(q32_t a, q32_t b)
{
	uint64_t al = (uint32_t)a;
	uint64_t ah = (uint64_t)a >> 32;
	uint64_t bl = (uint32_t)b;
	uint64_t bh = (uint64_t)b >> 32;

	uint64_t al_bl = al * bl;
	uint64_t ah_bl = ah * bl;
	uint64_t al_bh = al * bh;
	uint64_t ah_bh = ah * bh;

	uint64_t mid = ah_bl + al_bh + (al_bl >> 32);
	uint64_t res = (ah_bh << 32) + mid;

	if (a < 0)
		res -= (uint64_t)b << 32;

	if (b < 0)
		res -= (uint64_t)a << 32;

	return (q32_t)res;
}
#endif

static ALWAYS_INLINE CONST q16_t pg_math_clamp(q16_t val, q16_t min, q16_t max)
{
	if (val < min)
		return min;

	if (val > max)
		return max;

	return val;
}

static ALWAYS_INLINE uint64_t pg_math_isqrt64(uint64_t val)
{
	uint64_t res = 0;
	uint64_t bit = 1ULL << 62;

	while (bit > val)
		bit >>= 2;

	while (bit != 0) {
		if (val >= res + bit) {
			val -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}

		bit >>= 2;
	}

	return res;
}

static ALWAYS_INLINE q16_t pg_math_q16_sqrt(q16_t x)
{
	if (UNLIKELY(x <= 0))
		return 0;

	return (q16_t)pg_math_isqrt64((uint64_t)x << 16);
}

static ALWAYS_INLINE CONST q16_t pg_math_sigmoid(q16_t val, q16_t k, q16_t mid)
{
	q16_t x = q16_mul(k, val - mid);
	q16_t abs_x = (x >= 0) ? x : -x;
	q16_t denom = Q16_ONE + abs_x;
	q16_t frac = q16_div(x, denom);
	q16_t inner = frac + Q16_ONE;
	return inner >> 1;
}

static ALWAYS_INLINE CONST q16_t pg_math_decay(q16_t val, q16_t coeff)
{
	q16_t x = q16_mul(coeff, val);
	if (UNLIKELY(x < 0))
		return Q16_ONE;

	q16_t x_sq = q16_mul(x, x);
	q16_t half_x_sq = x_sq >> 1;
	q16_t denom = Q16_ONE + x + half_x_sq;
	return q16_div(Q16_ONE, denom);
}

static ALWAYS_INLINE CONST q16_t pg_math_alg_sig(q16_t x)
{
	if (x >= INT_TO_Q16(180))
		return Q16_ONE;

	if (x <= -INT_TO_Q16(180))
		return -Q16_ONE;

	q16_t x_sq = q16_mul(x, x);
	q16_t radicand = Q16_ONE + x_sq;

	q16_t denom = pg_math_q16_sqrt(radicand);
	if (UNLIKELY(denom == 0))
		return (x >= 0) ? Q16_ONE : -Q16_ONE;

	return q16_div(x, denom);
}

static ALWAYS_INLINE CONST uint64_t pg_math_san_u64(q16_t val,
						    uint64_t fallback)
{
	if (UNLIKELY(val < 0))
		return fallback;

	return (uint64_t)((val + Q16_HALF) >> Q16_SHIFT);
}

static ALWAYS_INLINE CONST uint64_t pg_math_san_quant_u64(q16_t val,
							  q16_t fallback,
							  uint64_t step)
{
	q16_t safe_val = (UNLIKELY(val < 0)) ? fallback : val;
	uint64_t val_ns = ((uint64_t)safe_val * 1000000ULL) >> Q16_SHIFT;

	if (UNLIKELY(step == 0 || step == 1))
		return val_ns;

	uint64_t remainder = val_ns % step;
	if (remainder >= step / 2)
		return val_ns + step - remainder;

	return val_ns - remainder;
}

#endif // PG_MATH_H
