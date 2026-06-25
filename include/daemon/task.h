// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_TASK_H
#define PASCAL_GOV_DAEMON_TASK_H

void pascal_gov_task_set_realtime(void);
void pascal_gov_task_set_io_priority(void);
void pascal_gov_task_enforce_efficiency(void);
void pascal_gov_task_maximize_timer_slack(void);
void pascal_gov_task_limit_uclamp(void);

#endif // PASCAL_GOV_DAEMON_TASK_H
