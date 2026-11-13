// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PATH_H
#define PGOV_PATH_H

#if defined(NDK_BUILD)
#define PATH_LOCK "/data/local/tmp/pgovd.lock"
#define PATH_LOG "/data/adb/modules/pgovd/pgovd.log"
#else
#define PATH_LOCK "/data/vendor/pgovd/pgovd.lock"
#define PATH_LOG "/data/vendor/pgovd/pgovd.log"
#endif

#if defined(NDK_BUILD)
#define PATH_CONF "/data/adb/modules/pgovd/system/etc/pgovd.conf"
#endif

#define PATH_PSI_CPU "/proc/pressure/cpu"
#define PATH_THERMAL_BASE "/sys/class/thermal"
#define PATH_BACKLIGHT_BASE "/sys/class/backlight"
#define PATH_LEDS_BASE "/sys/class/leds"
#define PATH_SCHED_LAT_DBG "/sys/kernel/debug/sched/latency_ns"
#define PATH_SCHED_GRAN_DBG "/sys/kernel/debug/sched/min_granularity_ns"
#define PATH_SCHED_WAKEUP_DBG "/sys/kernel/debug/sched/wakeup_granularity_ns"
#define PATH_SCHED_MIG_DBG "/sys/kernel/debug/sched/migration_cost_ns"
#define PATH_SCHED_LAT "/proc/sys/kernel/sched_latency_ns"
#define PATH_SCHED_GRAN "/proc/sys/kernel/sched_min_granularity_ns"
#define PATH_SCHED_WAKEUP "/proc/sys/kernel/sched_wakeup_granularity_ns"
#define PATH_SCHED_MIG "/proc/sys/kernel/sched_migration_cost_ns"
#define PATH_SCHED_WALT "/proc/sys/kernel/sched_walt_init_task_load_pct"
#define PATH_SCHED_UCLAMP_UTIL "/proc/sys/kernel/sched_uclamp_util_min"
#define PATH_SCHED_UTIL_CLAMP "/proc/sys/kernel/sched_util_clamp_min"
#define PATH_BATTERY_TEMP "/sys/class/power_supply/battery/temp"
#define PATH_BATTERY_CAP "/sys/class/power_supply/battery/capacity"

#endif // PGOV_PATH_H
