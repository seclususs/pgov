// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PATHS_H
#define PGOV_PATHS_H

#if defined(NDK_BUILD)
#define PG_PATH_LOCK "/data/local/tmp/pgovd.lock"
#else
#define PG_PATH_LOCK "/data/vendor/pgovd/pgovd.lock"
#endif

#if defined(NDK_BUILD)
#define CONF_PATH "/data/adb/modules/pgovd/system/etc/pgovd.conf"
#endif

#define PG_PATH_PSI_CPU "/proc/pressure/cpu"
#define PG_PATH_THERMAL_BASE "/sys/class/thermal"
#define PG_PATH_BACKLIGHT_BASE "/sys/class/backlight"
#define PG_PATH_SCHED_LAT "/proc/sys/kernel/sched_latency_ns"
#define PG_PATH_SCHED_GRAN "/proc/sys/kernel/sched_min_granularity_ns"
#define PG_PATH_SCHED_WAKEUP "/proc/sys/kernel/sched_wakeup_granularity_ns"
#define PG_PATH_SCHED_MIG "/proc/sys/kernel/sched_migration_cost_ns"
#define PG_PATH_SCHED_WALT "/proc/sys/kernel/sched_walt_init_task_load_pct"
#define PG_PATH_SCHED_UCLAMP "/proc/sys/kernel/sched_uclamp_util_min"
#define PG_PATH_BATTERY_TEMP "/sys/class/power_supply/battery/temp"
#define PG_PATH_BATTERY_CAP "/sys/class/power_supply/battery/capacity"

#endif // PGOV_PATHS_H
