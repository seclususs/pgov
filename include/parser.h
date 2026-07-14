// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARSER_H
#define PGOV_PARSER_H

#include "compiler.h"
#include "pg/math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static ALWAYS_INLINE size_t pg_fmt_u64(uint64_t val, char *buf)
{
	if (val == 0) {
		buf[0] = '0';
		buf[1] = '\n';
		return 2;
	}

	char tmp[32];
	char *p = &tmp[31];
	*p = '\n';

	while (val > 0) {
		*(--p) = (char)((val % 10) + '0');
		val /= 10;
	}

	size_t len = (size_t)(&tmp[31] - p) + 1;
	for (size_t i = 0; i < len; ++i)
		buf[i] = p[i];

	return len;
}

static ALWAYS_INLINE size_t pg_fmt_u32(uint32_t val, char *buf)
{
	if (val == 0) {
		buf[0] = '0';
		return 1;
	}

	char tmp[16];
	char *p = &tmp[16];

	while (val > 0) {
		*(--p) = (char)((val % 10) + '0');
		val /= 10;
	}

	size_t len = (size_t)(&tmp[16] - p);
	for (size_t i = 0; i < len; ++i)
		buf[i] = p[i];

	return len;
}

static ALWAYS_INLINE int32_t pg_parse_i32(const uint8_t *RESTRICT buf,
					  size_t len, bool *RESTRICT valid)
{
	uint64_t val = 0;
	int32_t sign = 1;
	*valid = false;

	for (size_t i = 0; i < len; ++i) {
		uint8_t b = buf[i];

		if (b >= '0' && b <= '9') {
			if (val <= (uint64_t)2147483648ULL)
				val = (val * 10ULL) + (uint64_t)(b - '0');

			*valid = true;
			continue;
		}

		if (b == '-' && !(*valid)) {
			sign = -1;
			continue;
		}

		if (*valid)
			break;
	}

	int64_t res = (int64_t)val * sign;

	if (res > INT32_MAX)
		return INT32_MAX;

	if (res < INT32_MIN)
		return INT32_MIN;

	return (int32_t)res;
}

static ALWAYS_INLINE uint64_t pg_parse_u64(const uint8_t *RESTRICT buf,
					   size_t len, size_t start,
					   size_t *RESTRICT next)
{
	size_t pos = start;
	uint64_t val = 0;

	while (pos < len) {
		uint8_t b = buf[pos];

		if (b < '0' || b > '9')
			break;

		val = (val * 10ULL) + (uint64_t)(b - '0');
		pos++;
	}

	*next = pos;
	return val;
}

static ALWAYS_INLINE q16_t pg_parse_q16(const uint8_t *RESTRICT buf, size_t len,
					size_t start, size_t *RESTRICT next)
{
	size_t pos = start;
	q16_t val = 0;
	uint32_t f_val = 0;
	uint32_t f_div = 1;
	bool frac = false;

	while (pos < len) {
		uint8_t b = buf[pos];

		if (b == '.') {
			frac = true;
			pos++;
			continue;
		}

		if (b < '0' || b > '9')
			break;

		if (!frac) {
			val = q16_mul(val, INT_TO_Q16(10)) +
			      INT_TO_Q16(b - '0');
			pos++;
			continue;
		}

		if (LIKELY(f_div < 1000000)) {
			f_val = (f_val * 10) + (b - '0');
			f_div *= 10;
		}

		pos++;
	}

	*next = pos;

	if (frac && f_div > 1) {
		q16_t f_q16 = (q16_t)(((uint64_t)f_val << 16) / f_div);
		return val + f_q16;
	}

	return val;
}

#endif // PGOV_PARSER_H
