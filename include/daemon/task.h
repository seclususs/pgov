// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_TASK_H
#define PASCAL_GOV_DAEMON_TASK_H

#include <stdint.h>

int pascal_gov_task_set_realtime(int priority);
int pascal_gov_task_set_io_priority(int ioprio_class, int ioprio_data);
int pascal_gov_task_enforce_efficiency(void);
int pascal_gov_task_maximize_timer_slack(unsigned long slack_ns);
int pascal_gov_task_limit_uclamp(uint32_t util_max);

#endif // PASCAL_GOV_DAEMON_TASK_H
