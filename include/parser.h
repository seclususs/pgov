// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARSER_H
#define PGOV_PARSER_H

#include "compiler.h"
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

	char temp[32];
	char *p = &temp[31];
	*p = '\n';

	while (val > 0) {
		*(--p) = (char)((val % 10) + '0');
		val /= 10;
	}

	size_t len = (size_t)(&temp[31] - p) + 1;
	for (size_t i = 0; i < len; ++i)
		buf[i] = p[i];

	return len;
}

static ALWAYS_INLINE size_t pg_fmt_u32(int32_t val, char *buf)
{
	char temp[16];
	size_t i = 0;

	if (val <= 0) {
		buf[0] = '0';
		return 1;
	}

	while (val > 0) {
		temp[i++] = (char)((val % 10) + '0');
		val /= 10;
	}

	size_t len = i;
	for (size_t j = 0; j < len; ++j)
		buf[j] = temp[len - 1 - j];

	return len;
}

static ALWAYS_INLINE int32_t pg_parse_i32(const uint8_t *RESTRICT buf,
					  size_t len, bool *RESTRICT valid)
{
	int32_t val = 0;
	int32_t sign = 1;
	*valid = false;

	for (size_t i = 0; i < len; ++i) {
		uint8_t byte = buf[i];

		if (byte >= '0' && byte <= '9') {
			val = (val * 10) + (byte - '0');
			*valid = true;
			continue;
		}

		if (byte == '-' && !(*valid)) {
			sign = -1;
			continue;
		}

		if (*valid)
			break;
	}

	return val * sign;
}

static ALWAYS_INLINE uint64_t pg_parse_u64(const uint8_t *RESTRICT buf,
					   size_t len, size_t start_pos,
					   size_t *RESTRICT next)
{
	size_t pos = start_pos;
	uint64_t val = 0;

	while (pos < len) {
		uint8_t byte = buf[pos];

		if (byte >= '0' && byte <= '9') {
			val = (val * 10ULL) + (uint64_t)(byte - '0');
			pos++;
			continue;
		}

		break;
	}

	*next = pos;
	return val;
}

static ALWAYS_INLINE float pg_parse_f32(const uint8_t *RESTRICT buffer,
					size_t len, size_t start_pos,
					size_t *RESTRICT next)
{
	size_t pos = start_pos;
	float val = 0.0F;
	float frac_val = 0.0F;
	float frac_div = 1.0F;
	bool frac = false;

	while (pos < len) {
		uint8_t byte = buffer[pos];

		if (byte >= '0' && byte <= '9') {
			float digit = (float)(byte - '0');

			if (frac) {
				frac_val = (frac_val * 10.0F) + digit;
				frac_div *= 10.0F;
			} else {
				val = (val * 10.0F) + digit;
			}

			pos++;
			continue;
		}

		if (byte == '.') {
			frac = true;
			pos++;
			continue;
		}

		break;
	}

	*next = pos;
	return val + (frac_val / frac_div);
}

#endif // PGOV_PARSER_H
