// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_MATH_H
#define PGOV_MATH_H

#include "pgov/compiler.h"
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

static ALWAYS_INLINE q16_t math_abs_q16(q16_t val)
{
	if (UNLIKELY(val == INT32_MIN))
		return INT32_MAX;

	return (val < 0) ? -val : val;
}

static ALWAYS_INLINE q16_t math_mul_q16(q16_t a, q16_t b)
{
	return (q16_t)(((q32_t)a * b) >> Q16_SHIFT);
}

static ALWAYS_INLINE q16_t math_div_q16(q16_t a, q16_t b)
{
	if (UNLIKELY(b == 0))
		return (a >= 0) ? INT32_MAX : INT32_MIN;

	return (q16_t)(((q32_t)a * Q16_ONE) / b);
}

#ifdef __SIZEOF_INT128__
static ALWAYS_INLINE q32_t math_mul_q32(q32_t a, q32_t b)
{
	return (q32_t)(((unsigned __int128)a * (unsigned __int128)b) >> 32);
}
#else
static ALWAYS_INLINE q32_t math_mul_q32(q32_t a, q32_t b)
{
	uint64_t al = (uint32_t)a;
	uint64_t ah = (uint64_t)a >> 32;
	uint64_t bl = (uint32_t)b;
	uint64_t bh = (uint64_t)b >> 32;

	uint64_t ll = al * bl;
	uint64_t hl = ah * bl;
	uint64_t lh = al * bh;
	uint64_t hh = ah * bh;

	uint64_t mid = hl + lh + (ll >> 32);
	uint64_t res = (hh << 32) + mid;

	if (a < 0)
		res -= (uint64_t)b << 32;

	if (b < 0)
		res -= (uint64_t)a << 32;

	return (q32_t)res;
}
#endif

static ALWAYS_INLINE CONST q16_t math_clamp(q16_t val, q16_t min, q16_t max)
{
	if (val < min)
		return min;

	if (val > max)
		return max;

	return val;
}

static ALWAYS_INLINE uint64_t math_isqrt64(uint64_t val)
{
	uint64_t res = 0;
	uint64_t bit;
	int shift;

	if (UNLIKELY(val == 0))
		return 0;

	shift = (63 - __builtin_clzll(val)) & ~1;
	bit = 1ULL << shift;

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

static ALWAYS_INLINE q16_t math_sqrt_q16(q16_t val)
{
	if (UNLIKELY(val <= 0))
		return 0;

	return (q16_t)math_isqrt64((uint64_t)val << 16);
}

static ALWAYS_INLINE CONST q16_t math_sigmoid(q16_t val, q16_t k, q16_t mid)
{
	q16_t x;
	q16_t abs_x;
	q16_t den;
	q16_t frac;

	x = math_mul_q16(k, val - mid);
	abs_x = (x >= 0) ? x : -x;
	den = Q16_ONE + abs_x;
	frac = math_div_q16(x, den);

	return (frac + Q16_ONE) >> 1;
}

static ALWAYS_INLINE CONST q16_t math_decay(q16_t val, q16_t coeff)
{
	q16_t x;
	q16_t sq;
	q16_t den;

	x = math_mul_q16(coeff, val);

	if (UNLIKELY(x < 0))
		return Q16_ONE;

	if (UNLIKELY(x >= INT_TO_Q16(180)))
		return 0;

	sq = math_mul_q16(x, x);
	den = Q16_ONE + x + (sq >> 1);

	return math_div_q16(Q16_ONE, den);
}

static ALWAYS_INLINE CONST q16_t math_algebraic_sigmoid(q16_t val)
{
	q16_t sq;
	q16_t den;

	if (val >= INT_TO_Q16(180))
		return Q16_ONE;

	if (val <= -INT_TO_Q16(180))
		return -Q16_ONE;

	sq = math_mul_q16(val, val);
	den = math_sqrt_q16(Q16_ONE + sq);

	if (UNLIKELY(den == 0))
		return (val >= 0) ? Q16_ONE : -Q16_ONE;

	return math_div_q16(val, den);
}

static ALWAYS_INLINE CONST uint64_t math_sanitize_u64(q16_t val,
						      uint64_t fallback)
{
	if (UNLIKELY(val < 0))
		return fallback;

	return (uint64_t)((val + Q16_HALF) >> Q16_SHIFT);
}

static ALWAYS_INLINE CONST uint64_t math_quantize_u64(q16_t val, q16_t fallback,
						      uint64_t step)
{
	uint64_t ns;
	uint64_t rem;

	if (UNLIKELY(val < 0))
		val = fallback;

	ns = ((uint64_t)val * 1000000ULL) >> Q16_SHIFT;

	if (UNLIKELY(step <= 1))
		return ns;

	if (LIKELY((step & (step - 1)) == 0))
		return (ns + (step >> 1)) & ~(step - 1);

	rem = ns % step;
	if (rem >= (step >> 1))
		return ns + step - rem;

	return ns - rem;
}

static ALWAYS_INLINE CONST q16_t math_lerp(q16_t min, q16_t max, q16_t val)
{
	q16_t range = max - min;

	return min + math_mul_q16(range, val);
}

#endif // PGOV_MATH_H
