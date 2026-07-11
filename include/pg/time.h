// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_TIME_H
#define PG_TIME_H

#include "compiler.h"
#include "pg/math.h"
#include <time.h>

static ALWAYS_INLINE q16_t pg_dt_sec(const struct timespec *RESTRICT prev,
				     const struct timespec *RESTRICT now)
{
	int64_t sec = (int64_t)(now->tv_sec - prev->tv_sec);
	int64_t nsec = (int64_t)(now->tv_nsec - prev->tv_nsec);

	if (UNLIKELY(sec > 32000))
		sec = 32000;

	if (UNLIKELY(sec < -32000))
		sec = -32000;

	q16_t nsec_q16 = (q16_t)((nsec * Q16_ONE) / 1000000000LL);
	return (q16_t)(sec * Q16_ONE) + nsec_q16;
}

#endif // PG_TIME_H
