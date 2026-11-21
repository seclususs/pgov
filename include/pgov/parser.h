// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARSER_H
#define PGOV_PARSER_H

#include "pgov/compiler.h"
#include "pgov/math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static ALWAYS_INLINE size_t parse_fmt_u64(uint64_t val, char *buf)
{
	char tmp[32];
	char *cur;
	size_t i;
	size_t len;

	if (UNLIKELY(!buf))
		return 0;

	if (val == 0) {
		buf[0] = '0';
		buf[1] = '\n';
		return 2;
	}

	cur = &tmp[31];
	*cur = '\n';

	while (val > 0) {
		*(--cur) = (char)((val % 10) + '0');
		val /= 10;
	}

	len = (size_t)(&tmp[31] - cur) + 1;
	for (i = 0; i < len; ++i)
		buf[i] = cur[i];

	return len;
}

static ALWAYS_INLINE size_t parse_fmt_u32(uint32_t val, char *buf)
{
	char tmp[16];
	char *cur;
	size_t i;
	size_t len;

	if (UNLIKELY(!buf))
		return 0;

	if (val == 0) {
		buf[0] = '0';
		return 1;
	}

	cur = &tmp[16];

	while (val > 0) {
		*(--cur) = (char)((val % 10) + '0');
		val /= 10;
	}

	len = (size_t)(&tmp[16] - cur);
	for (i = 0; i < len; ++i)
		buf[i] = cur[i];

	return len;
}

static ALWAYS_INLINE int32_t parse_i32(const uint8_t *RESTRICT buf, size_t len,
				       bool *RESTRICT valid)
{
	int64_t res;
	int32_t sign = 1;
	size_t i;
	uint64_t val = 0;
	uint8_t byte;

	if (UNLIKELY(!buf || !valid)) {
		if (valid)
			*valid = false;

		return 0;
	}

	*valid = false;

	for (i = 0; i < len; ++i) {
		byte = buf[i];

		if (byte >= '0' && byte <= '9') {
			if (val <= (uint64_t)2147483648ULL)
				val = (val * 10ULL) + (uint64_t)(byte - '0');

			*valid = true;
			continue;
		}

		if (byte == '-' && !(*valid)) {
			sign = -1;
			continue;
		}

		if (byte == '\n' || byte == '\r' || byte == ' ' ||
		    byte == '\t') {
			if (*valid)
				break;

			continue;
		}

		*valid = false;
		return 0;
	}

	res = (int64_t)val * sign;

	if (res > INT32_MAX)
		return INT32_MAX;

	if (res < INT32_MIN)
		return INT32_MIN;

	return (int32_t)res;
}

static ALWAYS_INLINE uint64_t parse_u64(const uint8_t *RESTRICT buf, size_t len,
					size_t start, size_t *RESTRICT next)
{
	size_t pos = start;
	uint64_t val = 0;
	uint8_t byte;

	if (UNLIKELY(!buf || !next)) {
		if (next)
			*next = start;

		return 0;
	}

	while (pos < len) {
		byte = buf[pos];

		if (byte < '0' || byte > '9')
			break;

		if (val <= 1844674407370955161ULL)
			val = (val * 10ULL) + (uint64_t)(byte - '0');

		pos++;
	}

	*next = pos;
	return val;
}

static ALWAYS_INLINE q16_t parse_q16(const uint8_t *RESTRICT buf, size_t len,
				     size_t start, size_t *RESTRICT next)
{
	size_t pos = start;
	uint32_t frac_div = 1;
	uint32_t frac_val = 0;
	uint32_t int_val = 0;
	uint8_t byte;
	bool frac = false;
	q16_t frac_q16;
	q16_t val;

	if (UNLIKELY(!buf || !next)) {
		if (next)
			*next = start;

		return 0;
	}

	while (pos < len) {
		byte = buf[pos];

		if (byte == '.') {
			frac = true;
			pos++;
			continue;
		}

		if (byte < '0' || byte > '9')
			break;

		if (!frac) {
			int_val = (int_val * 10) + (byte - '0');
			pos++;
			continue;
		}

		if (LIKELY(frac_div < 1000000)) {
			frac_val = (frac_val * 10) + (byte - '0');
			frac_div *= 10;
		}

		pos++;
	}

	*next = pos;

	val = (q16_t)(int_val << 16);
	if (frac && frac_div > 1) {
		frac_q16 = (q16_t)(((uint64_t)frac_val << 16) / frac_div);
		return val + frac_q16;
	}

	return val;
}

#endif // PGOV_PARSER_H
