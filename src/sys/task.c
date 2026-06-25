// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "daemon/task.h"
#include "daemon/logger.h"
#include "daemon/topology.h"
#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_ioprio_set
#if defined(__aarch64__)
#define __NR_ioprio_set 30
#else
#define __NR_ioprio_set 251
#endif
#endif

#ifndef __NR_sched_setattr
#if defined(__aarch64__)
#define __NR_sched_setattr 274
#else
#define __NR_sched_setattr 380
#endif
#endif

#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif

#ifndef SCHED_FLAG_KEEP_POLICY
#define SCHED_FLAG_KEEP_POLICY 0x08
#endif

#ifndef SCHED_FLAG_UTIL_CLAMP_MAX
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif

#define RT_PRIORITY 50
#define TIMER_SLACK_NS (50UL * 1000UL * 1000UL)
#define UCLAMP_MAX 102

struct pascal_gov_sched_attr {
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;
	int32_t sched_nice;
	uint32_t sched_priority;
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;
	uint32_t sched_util_min;
	uint32_t sched_util_max;
};

void pascal_gov_task_set_realtime(void)
{
	struct sched_param param = {0};
	param.sched_priority = RT_PRIORITY;

	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
		LOGE("task: failed to set realtime sched_fifo err=%d", errno);
	else
		LOGD("task: realtime sched_fifo active");
}

void pascal_gov_task_set_io_priority(void)
{
	const int ioprio_val = (2 << 13) | 0;

	if (syscall(__NR_ioprio_set, IOPRIO_WHO_PROCESS, 0, ioprio_val) == -1)
		LOGE("task: failed to elevate i/o priority err=%d", errno);
	else
		LOGD("task: best-effort i/o priority active");
}

void pascal_gov_task_enforce_efficiency(void)
{
	if (pascal_gov_topology_apply_little_core() == -1)
		LOGE("task: failed to apply topology affinity err=%d", errno);
	else
		LOGD("task: topology affinity active");
}

void pascal_gov_task_maximize_timer_slack(void)
{
	const unsigned long slack_ns = TIMER_SLACK_NS;

	if (prctl(PR_SET_TIMERSLACK, slack_ns) == -1)
		LOGW("task: failed to relax timer slack err=%d", errno);
	else
		LOGD("task: timer coalescing active");
}

void pascal_gov_task_limit_uclamp(void)
{
	struct pascal_gov_sched_attr attr = {0};
	attr.size = sizeof(attr);
	attr.sched_flags = SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_UTIL_CLAMP_MAX;
	attr.sched_util_max = UCLAMP_MAX;

	if (syscall(__NR_sched_setattr, 0, &attr, 0) == -1)
		LOGW("task: failed to set uclamp max limit err=%d", errno);
	else
		LOGD("task: uclamp-max limitation active");
}
