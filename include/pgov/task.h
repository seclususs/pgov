// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TASK_H
#define PGOV_TASK_H

#include <stdint.h>

#define TASK_UCLAMP_MAX 102

int task_set_rt_priority(int priority);
int task_set_io_priority(int ioprio_class, int ioprio_data);
int task_set_core_affinity(void);
int task_set_timer_slack(unsigned long slack_ns);
int task_set_uclamp(uint32_t util_max);
int task_protect_oom(void);

#endif // PGOV_TASK_H
