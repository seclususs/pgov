// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PARAM_H
#define PGOV_PARAM_H

#include "pgov/cpu.h"
#include "pgov/thermal.h"

extern struct pg_cpu_lim param_sched;
extern struct pg_cpu_cfg param_cpu;
extern struct pg_thermal_cfg param_thermal;

#endif // PGOV_PARAM_H
