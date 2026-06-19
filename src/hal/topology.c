// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "daemon/topology.h"
#include "daemon/logger.h"
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define CPU_CAPACITY "/sys/devices/system/cpu/cpu%d/cpu_capacity"
#define CPU_MAX_FREQ "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq"

static long read_sysfs_long(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	char buf[32];
	ssize_t bytes = read(fd, buf, sizeof(buf));

	close(fd);

	if (bytes <= 0)
		return -1;

	long val = 0;

	for (ssize_t i = 0; i < bytes; ++i) {
		if (buf[i] >= '0' && buf[i] <= '9')
			val = val * 10 + (buf[i] - '0');
		else
			break;
	}

	return val;
}

static bool build_little_cpuset(long num_cores, const char *fmt,
				cpu_set_t *cpuset)
{
	long min_val = LONG_MAX;
	bool found = false;
	char path_buf[128];

	CPU_ZERO(cpuset);

	for (int i = 0; i < num_cores; ++i) {
		(void)snprintf(path_buf, sizeof(path_buf), fmt, i);

		long val = read_sysfs_long(path_buf);
		if (val <= 0)
			continue;

		found = true;

		if (val < min_val) {
			min_val = val;
			CPU_ZERO(cpuset);
			CPU_SET(i, cpuset);
		} else if (val == min_val) {
			CPU_SET(i, cpuset);
		}
	}

	return found;
}

int pascal_gov_topology_apply_little_core(void)
{
	long num_cores = sysconf(_SC_NPROCESSORS_CONF);
	if (num_cores <= 0)
		return -1;

	cpu_set_t little_cpuset;

	bool success =
		build_little_cpuset(num_cores, CPU_CAPACITY, &little_cpuset) ||
		build_little_cpuset(num_cores, CPU_MAX_FREQ, &little_cpuset);

	if (!success) {
		LOGW("topology: detection failed, binding to all cores");

		CPU_ZERO(&little_cpuset);
		for (int i = 0; i < num_cores; ++i)
			CPU_SET(i, &little_cpuset);
	}

	return sched_setaffinity(0, sizeof(cpu_set_t), &little_cpuset);
}
