// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_TIME_H
#define PG_TIME_H

#include "compiler.h"
#include <time.h>

static ALWAYS_INLINE float pg_dt_sec(const struct timespec *RESTRICT prev,
				     const struct timespec *RESTRICT now)
{
	float sec = (float)(now->tv_sec - prev->tv_sec);
	float nsec = (float)(now->tv_nsec - prev->tv_nsec) * 1e-9F;
	return sec + nsec;
}

#endif // PG_TIME_H
