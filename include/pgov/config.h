// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_CONFIG_H
#define PGOV_CONFIG_H

#include "pgov/cpu.h"
#include "pgov/thermal.h"

#define CFG_MIN_POLL_MS 100
#define CFG_MAX_POLL_MS 10000
#define CFG_MAX_EVENTS 16
#define CFG_STAB_DELAY_SEC 10
#define CFG_BOOT_RETRY_MAX 300
#define CFG_BOOT_POLL_SEC 1
#define CFG_QUANT_NS 65536ULL
#define CFG_CHK_LAT 100
#define CFG_CHK_GRAN 100
#define CFG_CHK_WAKE 150
#define CFG_CHK_MIG 50000
#define CFG_CHK_WALT 5
#define CFG_CHK_UCL 32
#define CFG_RT_PRIORITY 50
#define CFG_UCLAMP_MAX 102
#define CFG_STACK_SIZE (512UL * 1024UL)
#define CFG_TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define CFG_IOPRIO_CLASS 2
#define CFG_IOPRIO_DATA 0
#define CFG_BAT_CHK_SEC 5
#define CFG_THERM_CHK_SEC 1
#define CFG_DISP_CHK_SEC 1
#define CFG_THERM_STALE_SEC 30
#define CFG_TEMP_FAIL_MAX 10
#define CFG_PSI_FAIL_MAX 6
#define CFG_NODE_FAIL_MAX 7
#define CFG_BL_MAX 4
#define CFG_PSI_THRESHOLD_US 100000
#define CFG_PSI_WINDOW_US 1000000

#if defined(NDK_BUILD)
#define CFG_SWEEP_IVL_SEC 600
#endif

extern struct pg_cpu_lim LIM_CPU;
extern struct pg_cpu_cfg CFG_CPU;
extern struct pg_thermal_cfg CFG_THERMAL;

#endif // PGOV_CONFIG_H
