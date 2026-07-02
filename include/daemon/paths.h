// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_PATHS_H
#define PASCAL_GOV_DAEMON_PATHS_H

#define PASCAL_GOV_LOCK_PATH "/data/local/tmp/pascal_gov.lock"

#define PASCAL_GOV_PATH_PSI_CPU "/proc/pressure/cpu"

#define PASCAL_GOV_PATH_SCHED_LATENCY_NS "/proc/sys/kernel/sched_latency_ns"

#define PASCAL_GOV_PATH_SCHED_MIN_GRANULARITY_NS                               \
	"/proc/sys/kernel/sched_min_granularity_ns"

#define PASCAL_GOV_PATH_SCHED_WAKEUP_GRANULARITY_NS                            \
	"/proc/sys/kernel/sched_wakeup_granularity_ns"

#define PASCAL_GOV_PATH_SCHED_MIGRATION_COST_NS                                \
	"/proc/sys/kernel/sched_migration_cost_ns"

#define PASCAL_GOV_PATH_SCHED_WALT_INIT                                        \
	"/proc/sys/kernel/sched_walt_init_task_load_pct"

#define PASCAL_GOV_PATH_SCHED_UCLAMP_MIN                                       \
	"/proc/sys/kernel/sched_uclamp_util_min"

#define PASCAL_GOV_PATH_BATTERY_TEMP "/sys/class/power_supply/battery/temp"

#define PASCAL_GOV_PATH_BATTERY_CAPACITY                                       \
	"/sys/class/power_supply/battery/capacity"

#endif // PASCAL_GOV_DAEMON_PATHS_H
