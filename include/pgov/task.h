// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TASK_H
#define PGOV_TASK_H

#include <stddef.h>
#include <stdint.h>

#define TASK_UCLAMP_MAX 102

/*
 * Set real-time FIFO scheduling policy (SCHED_FIFO) and priority
 *
 * Returns 0 on success, or -errno on failure
 */
int task_set_rt_priority(int priority);

int task_set_io_priority(int ioprio_class, int ioprio_data);
int task_set_core_affinity(void);

/* Configure timer slack via prctl(PR_SET_TIMERSLACK) for wake precision */
int task_set_timer_slack(unsigned long slack_ns);

int task_set_uclamp(uint32_t util_max);

/* Protect daemon from Linux OOM killer by writing to /proc/self/oom_score_adj */
int task_protect_oom(void);

int task_set_max_fd(void);
int task_set_stack_limit(size_t bytes);

#endif // PGOV_TASK_H
