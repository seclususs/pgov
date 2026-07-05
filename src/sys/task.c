// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "task.h"
#include "pg/log.h"
#include "topo.h"
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

struct pg_sched_attr {
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

int pg_task_set_rt_prio(int priority)
{
	struct sched_param param = { 0 };
	param.sched_priority = priority;

	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		int err = errno;
		LOGE("task: failed to set realtime sched_fifo err=%d", err);
		return -err;
	}

	LOGD("task: realtime sched_fifo active prio=%d", priority);
	return 0;
}

int pg_task_set_io_prio(int ioprio_class, int ioprio_data)
{
	const int ioprio_val = (ioprio_class << 13) | ioprio_data;

	if (syscall(__NR_ioprio_set, IOPRIO_WHO_PROCESS, 0, ioprio_val) == -1) {
		int err = errno;
		LOGE("task: failed to elevate i/o priority err=%d", err);
		return -err;
	}

	LOGD("task: i/o priority active class=%d", ioprio_class);
	return 0;
}

int pg_task_set_core_efficiency(void)
{
	if (pg_topo_apply_little_core() == -1) {
		int err = errno;
		LOGE("task: failed to apply topology affinity err=%d", err);
		return -err;
	}

	LOGD("task: topology affinity active");
	return 0;
}

int pg_task_set_timer_slack(unsigned long slack_ns)
{
	if (prctl(PR_SET_TIMERSLACK, slack_ns) == -1) {
		int err = errno;
		LOGW("task: failed to relax timer slack err=%d", err);
		return -err;
	}

	LOGD("task: timer coalescing active slack=%lu ns", slack_ns);
	return 0;
}

int pg_task_set_uclamp(uint32_t util_max)
{
	struct pg_sched_attr attr = { 0 };
	attr.size = sizeof(attr);
	attr.sched_flags = SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_UTIL_CLAMP_MAX;
	attr.sched_util_max = util_max;

	if (syscall(__NR_sched_setattr, 0, &attr, 0) == -1) {
		int err = errno;
		LOGW("task: failed to set uclamp max limit err=%d", err);
		return -err;
	}

	LOGD("task: uclamp-max limitation active max=%u", util_max);
	return 0;
}
