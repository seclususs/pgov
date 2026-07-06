// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "topo.h"
#include "pg/log.h"
#include "compiler.h"
#include "sysfs.h"
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define CPU_CAPACITY "/sys/devices/system/cpu/cpu%d/cpu_capacity"
#define CPU_MAX_FREQ "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq"

static bool build_cpuset(long num_cores, const char *RESTRICT fmt,
			 cpu_set_t *RESTRICT cpuset)
{
	long min_val = INT32_MAX;
	bool found = false;
	char buf[128];

	CPU_ZERO(cpuset);

	for (int i = 0; i < num_cores; ++i) {
		(void)snprintf(buf, sizeof(buf), fmt, i);

		int32_t val = pg_sysfs_read_i32(buf);
		if (val <= 0)
			continue;

		found = true;

		if (val < min_val) {
			min_val = val;
			CPU_ZERO(cpuset);
			CPU_SET(i, cpuset);
		} else if (val == min_val)
			CPU_SET(i, cpuset);
	}

	return found;
}

int pg_topo_apply_little_core(void)
{
	long num_cores = sysconf(_SC_NPROCESSORS_CONF);
	if (num_cores <= 0)
		return -1;

	cpu_set_t little_cpuset;

	bool success =
		(bool)(build_cpuset(num_cores, CPU_CAPACITY, &little_cpuset) ||
		       build_cpuset(num_cores, CPU_MAX_FREQ, &little_cpuset));

	if (!success) {
		LOGW("topology: detection failed, binding to all cores");

		CPU_ZERO(&little_cpuset);
		for (int i = 0; i < num_cores; ++i)
			CPU_SET(i, &little_cpuset);
	}

	return sched_setaffinity(0, sizeof(cpu_set_t), &little_cpuset);
}
