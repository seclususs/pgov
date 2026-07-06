// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_STR_H
#define PGOV_STR_H

#include "compiler.h"
#include <stdbool.h>
#include <stddef.h>

static inline bool pg_str_has_prefix(const char *RESTRICT str,
				     const char *RESTRICT prefix)
{
	while (*prefix)
		if (*prefix++ != *str++)
			return false;

	return true;
}

static inline bool pg_str_contains(const char *RESTRICT str,
				   const char *RESTRICT substr)
{
	if (!*substr)
		return true;

	const char *p1;
	const char *p2;

	for (const char *p1_adv = str; *p1_adv; ++p1_adv) {
		p1 = p1_adv;
		p2 = substr;

		while (*p1 && *p2 && *p1 == *p2) {
			p1++;
			p2++;
		}

		if (!*p2)
			return true;
	}

	return false;
}

static inline void pg_str_to_lower(char *str)
{
	for (; *str; ++str)
		if (*str >= 'A' && *str <= 'Z')
			*str += 32;
}

static inline void pg_str_build_path(char *RESTRICT out, size_t len,
				     const char *RESTRICT dir,
				     const char *RESTRICT sub,
				     const char *RESTRICT file)
{
	if (UNLIKELY(len == 0))
		return;

	size_t i = 0;

	while (*dir && i + 1 < len)
		out[i++] = *dir++;

	if (i + 1 < len)
		out[i++] = '/';

	while (*sub && i + 1 < len)
		out[i++] = *sub++;

	if (i + 1 < len)
		out[i++] = '/';

	while (*file && i + 1 < len)
		out[i++] = *file++;

	out[i] = '\0';
}

#endif // PGOV_STR_H
