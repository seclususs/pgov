// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARAM_H
#define PGOV_PARAM_H

#include "pgov/sched.h"
#include "pgov/thermal.h"

extern struct sched_limit limit_sched;
extern struct sched_param param_sched;
extern struct thermal_param param_thermal;

#endif // PGOV_PARAM_H
