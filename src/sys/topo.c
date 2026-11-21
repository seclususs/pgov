// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/topo.h"
#include "pgov/compiler.h"
#include "pgov/log.h"
#include "pgov/node.h"
#include "pgov/parser.h"
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>

#define CPU_CAPACITY "/sys/devices/system/cpu/cpu%d/cpu_capacity"
#define CPU_MAX_FREQ "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq"

static void format_path(char *RESTRICT buf, size_t len,
			const char *RESTRICT fmt, int32_t cpu)
{
	size_t i = 0;
	size_t pos = 0;

	if (UNLIKELY(!buf || !fmt || len == 0))
		return;

	while (fmt[i] != '\0') {
		if (fmt[i] == '%' && fmt[i + 1] == 'd') {
			if (pos + 11 >= len)
				break;

			pos += parse_fmt_u32((uint32_t)cpu, buf + pos);
			i += 2;
			continue;
		}

		if (pos + 1 >= len)
			break;

		buf[pos++] = fmt[i++];
	}

	if (pos >= len)
		pos = len - 1;

	buf[pos] = '\0';
}

static bool build_cpuset(long cores, const char *fmt, cpu_set_t *cpuset)
{
	char buf[128];
	long min_val = INT32_MAX;
	int32_t val;
	bool found = false;
	int i;

	if (UNLIKELY(!cpuset || !fmt || cores <= 0))
		return false;

	CPU_ZERO(cpuset);

	if (cores > CPU_SETSIZE)
		cores = CPU_SETSIZE;

	for (i = 0; i < cores; ++i) {
		format_path(buf, sizeof(buf), fmt, i);

		if (node_read_i32(buf, &val) != 0 || val <= 0)
			continue;

		found = true;

		if (val > min_val)
			continue;

		if (val < min_val) {
			min_val = val;
			CPU_ZERO(cpuset);
		}

		CPU_SET(i, cpuset);
	}

	return found;
}

int topo_set_little_core(void)
{
	cpu_set_t cpuset;
	long cores;
	int i;

	cores = sysconf(_SC_NPROCESSORS_CONF);
	if (cores <= 0)
		return -EINVAL;

	if (build_cpuset(cores, CPU_CAPACITY, &cpuset) &&
	    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0)
		return 0;

	if (build_cpuset(cores, CPU_MAX_FREQ, &cpuset) &&
	    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0)
		return 0;

	LOGW("topology: detection failed, binding to all cores");

	if (cores > CPU_SETSIZE)
		cores = CPU_SETSIZE;

	CPU_ZERO(&cpuset);
	for (i = 0; i < cores; ++i)
		CPU_SET(i, &cpuset);

	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0)
		return -errno;

	return 0;
}

int32_t topo_get_core_count(void)
{
	long cores;

	cores = sysconf(_SC_NPROCESSORS_CONF);
	if (cores <= 0)
		return 0;

	return (int32_t)cores;
}

int32_t topo_get_maxfrq_khz(void)
{
	char buf[128];
	long cores;
	int32_t freq = 0;
	int32_t val;
	int i;

	cores = sysconf(_SC_NPROCESSORS_CONF);
	if (cores <= 0)
		return 0;

	for (i = 0; i < cores; ++i) {
		format_path(buf, sizeof(buf), CPU_MAX_FREQ, i);
		if (node_read_i32(buf, &val) == 0 && val > freq)
			freq = val;
	}

	return freq;
}

int topo_get_cpu_capacity(int32_t *RESTRICT total,
			  int32_t *RESTRICT min_capacity,
			  int32_t *RESTRICT max_capacity)
{
	char buf[128];
	long cores;
	int32_t min_val = INT32_MAX;
	int32_t max_val = 0;
	int32_t sum = 0;
	int32_t val;
	int i;

	cores = sysconf(_SC_NPROCESSORS_CONF);
	if (cores <= 0)
		return -EINVAL;

	for (i = 0; i < cores; ++i) {
		format_path(buf, sizeof(buf), CPU_CAPACITY, i);

		if (node_read_i32(buf, &val) != 0 || val <= 0)
			continue;

		sum += val;

		if (val < min_val)
			min_val = val;

		if (val > max_val)
			max_val = val;
	}

	if (sum == 0)
		return -ENODATA;

	if (total)
		*total = sum;

	if (min_capacity)
		*min_capacity = min_val;

	if (max_capacity)
		*max_capacity = max_val;

	return 0;
}
