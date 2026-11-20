// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_STR_H
#define PGOV_STR_H

#include "pgov/compiler.h"
#include <stdbool.h>
#include <stddef.h>

static inline bool str_has_prefix(const char *str, const char *prefix)
{
	if (UNLIKELY(!str || !prefix))
		return false;

	while (*prefix) {
		if (*prefix++ != *str++)
			return false;
	}

	return true;
}

static inline bool str_contains(const char *str, const char *substr)
{
	const char *adv;
	const char *p1;
	const char *p2;

	if (UNLIKELY(!str || !substr))
		return false;

	if (!*substr)
		return true;

	for (adv = str; *adv; ++adv) {
		p1 = adv;
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

static inline void str_to_lower(char *str)
{
	if (UNLIKELY(!str))
		return;

	for (; *str; ++str) {
		if (*str >= 'A' && *str <= 'Z')
			*str += 32;
	}
}

static inline void str_build_path(char *RESTRICT out, size_t len,
				  const char *RESTRICT dir,
				  const char *RESTRICT sub,
				  const char *RESTRICT file)
{
	size_t i = 0;

	if (UNLIKELY(!out || len == 0))
		return;

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
