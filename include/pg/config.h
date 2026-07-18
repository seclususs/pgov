// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_CONFIG_H
#define PG_CONFIG_H

#include "pg/cpu.h"
#include "pg/thermal.h"

#define PG_MIN_POLL_MS 100
#define PG_MAX_POLL_MS 10000
#define PG_MAX_EVENTS 16
#define PG_STAB_DELAY_SEC 10
#define PG_BOOT_RETRY_MAX 300
#define PG_BOOT_POLL_SEC 1
#define PG_QUANT_NS 65536ULL
#define PG_CHK_LAT 100
#define PG_CHK_GRAN 100
#define PG_CHK_WAKE 150
#define PG_CHK_MIG 50000
#define PG_CHK_WALT 5
#define PG_CHK_UCL 32
#define PG_RT_PRIORITY 50
#define PG_UCLAMP_MAX 102
#define PG_STACK_SIZE (512UL * 1024UL)
#define PG_TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define PG_IOPRIO_CLASS 2
#define PG_IOPRIO_DATA 0
#define PG_BAT_CHK_SEC 5
#define PG_PSI_THRESHOLD_US 100000
#define PG_PSI_WINDOW_US 1000000

#if defined(NDK_BUILD)
#define PG_SWEEP_IVL_SEC 600
#endif

extern struct pg_cpu_lim LIM_CPU;
extern struct pg_cpu_cfg CFG_CPU;
extern struct pg_thermal_cfg CFG_THERMAL;

#endif // PG_CONFIG_H
