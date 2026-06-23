// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_PARSER_H
#define PASCAL_GOV_PARSER_H

#include "pascal_gov/compiler.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline __attribute__((always_inline)) size_t
pascal_gov_fmt_u64(uint64_t val, char *out_buf)
{
	if (val == 0) {
		out_buf[0] = '0';
		out_buf[1] = '\n';
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
		out_buf[i] = p[i];

	return len;
}

static inline __attribute__((always_inline)) size_t
pascal_gov_fmt_u32(int32_t val, char *out_buf)
{
	char temp[16];
	size_t i = 0;

	if (val <= 0) {
		out_buf[0] = '0';
		return 1;
	}

	while (val > 0) {
		temp[i++] = (char)((val % 10) + '0');
		val /= 10;
	}

	size_t len = i;
	for (size_t j = 0; j < len; ++j)
		out_buf[j] = temp[len - 1 - j];

	return len;
}

static inline __attribute__((always_inline)) int32_t
pascal_gov_parse_i32(const uint8_t *PASCAL_GOV_RESTRICT buffer, size_t len,
		     bool *PASCAL_GOV_RESTRICT out_has_digits)
{
	int32_t parsed_val = 0;
	int32_t sign = 1;
	*out_has_digits = false;

	for (size_t i = 0; i < len; ++i) {
		uint8_t byte = buffer[i];

		if (byte >= '0' && byte <= '9') {
			parsed_val = (parsed_val * 10) + (byte - '0');
			*out_has_digits = true;
			continue;
		}

		if (byte == '-' && !(*out_has_digits)) {
			sign = -1;
			continue;
		}

		if (*out_has_digits)
			break;
	}

	return parsed_val * sign;
}

static inline __attribute__((always_inline)) uint64_t
pascal_gov_parse_u64(const uint8_t *PASCAL_GOV_RESTRICT buffer, size_t len,
		     size_t start_pos, size_t *PASCAL_GOV_RESTRICT out_next_pos)
{
	size_t pos = start_pos;
	uint64_t parsed_val = 0;

	while (pos < len) {
		uint8_t byte = buffer[pos];

		if (byte >= '0' && byte <= '9') {
			parsed_val =
				(parsed_val * 10ULL) + (uint64_t)(byte - '0');

			pos++;
			continue;
		}

		break;
	}

	*out_next_pos = pos;
	return parsed_val;
}

static inline __attribute__((always_inline)) float
pascal_gov_parse_f32(const uint8_t *PASCAL_GOV_RESTRICT buffer, size_t len,
		     size_t start_pos, size_t *PASCAL_GOV_RESTRICT out_next_pos)
{
	size_t pos = start_pos;
	float parsed_val = 0.0F;
	float fraction_val = 0.0F;
	float fraction_divisor = 1.0F;
	bool in_fraction = false;

	while (pos < len) {
		uint8_t byte = buffer[pos];

		if (byte >= '0' && byte <= '9') {
			float digit = (float)(byte - '0');

			if (in_fraction) {
				fraction_val = (fraction_val * 10.0F) + digit;
				fraction_divisor *= 10.0F;
			} else {
				parsed_val = (parsed_val * 10.0F) + digit;
			}

			pos++;
			continue;
		}

		if (byte == '.') {
			in_fraction = true;
			pos++;
			continue;
		}

		break;
	}

	*out_next_pos = pos;

	return parsed_val + (fraction_val / fraction_divisor);
}

#endif // PASCAL_GOV_PARSER_H
