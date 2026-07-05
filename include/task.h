// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TASK_H
#define PGOV_TASK_H

#include <stdint.h>

int pg_task_set_rt_prio(int priority);
int pg_task_set_io_prio(int ioprio_class, int ioprio_data);
int pg_task_set_core_efficiency(void);
int pg_task_set_timer_slack(unsigned long slack_ns);
int pg_task_set_uclamp(uint32_t util_max);

#endif // PGOV_TASK_H
