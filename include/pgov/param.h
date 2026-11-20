// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARAM_H
#define PGOV_PARAM_H

#include "pgov/cpu.h"
#include "pgov/thermal.h"

extern struct cpu_limit param_sched;
extern struct cpu_param param_cpu;
extern struct pg_thermal_cfg param_thermal;

#endif // PGOV_PARAM_H
