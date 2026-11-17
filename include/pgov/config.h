// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_CONFIG_H
#define PGOV_CONFIG_H

#include "pgov/cpu.h"
#include "pgov/thermal.h"

extern struct pg_cpu_lim LIM_CPU;
extern struct pg_cpu_cfg CFG_CPU;
extern struct pg_thermal_cfg CFG_THERMAL;

#endif // PGOV_CONFIG_H
