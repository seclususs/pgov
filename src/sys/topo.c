// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "topo.h"
#include "pg/log.h"
#include "compiler.h"
#include "parser.h"
#include "sysfs.h"
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>

#define CPU_CAPACITY "/sys/devices/system/cpu/cpu%d/cpu_capacity"
#define CPU_MAX_FREQ "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq"

static void format_path(char *RESTRICT buf, size_t buf_len,
			const char *RESTRICT fmt, int32_t cpu)
{
	size_t pos = 0;
	size_t i = 0;

	if (UNLIKELY(buf_len == 0))
		return;

	while (fmt[i] != '\0') {
		if (fmt[i] == '%' && fmt[i + 1] == 'd') {
			if (pos + 11 >= buf_len)
				break;

			pos += pg_fmt_u32(cpu, buf + pos);
			i += 2;
			continue;
		}

		buf[pos++] = fmt[i++];
	}

	buf[pos] = '\0';
}

static bool build_cpuset(long num_cores, const char *RESTRICT fmt,
			 cpu_set_t *RESTRICT cpuset)
{
	long min_val = INT32_MAX;
	bool found = false;
	char buf[128];
	int i;

	CPU_ZERO(cpuset);

	if (num_cores > CPU_SETSIZE)
		num_cores = CPU_SETSIZE;

	for (i = 0; i < num_cores; ++i) {
		int32_t val;

		format_path(buf, sizeof(buf), fmt, i);

		if (pg_sysfs_read_i32(buf, &val) != 0 || val <= 0)
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

int pg_topo_set_little_core(void)
{
	long num_cores = sysconf(_SC_NPROCESSORS_CONF);
	cpu_set_t little_cpuset;
	int i;

	if (num_cores <= 0)
		return -EINVAL;

	if (build_cpuset(num_cores, CPU_CAPACITY, &little_cpuset)) {
		if (sched_setaffinity(0, sizeof(cpu_set_t), &little_cpuset) < 0)
			return -errno;

		return 0;
	}

	if (build_cpuset(num_cores, CPU_MAX_FREQ, &little_cpuset)) {
		if (sched_setaffinity(0, sizeof(cpu_set_t), &little_cpuset) < 0)
			return -errno;

		return 0;
	}

	LOGW("topology: detection failed, binding to all cores");

	if (num_cores > CPU_SETSIZE)
		num_cores = CPU_SETSIZE;

	CPU_ZERO(&little_cpuset);
	for (i = 0; i < num_cores; ++i)
		CPU_SET(i, &little_cpuset);

	if (sched_setaffinity(0, sizeof(cpu_set_t), &little_cpuset) < 0)
		return -errno;

	return 0;
}

int32_t pg_topo_get_core_count(void)
{
	long cores = sysconf(_SC_NPROCESSORS_CONF);
	return (cores > 0) ? (int32_t)cores : 0;
}

int32_t pg_topo_get_max_freq_khz(void)
{
	long num_cores = sysconf(_SC_NPROCESSORS_CONF);
	int32_t freq = 0;
	char buf[128];
	int i;

	if (num_cores <= 0)
		return 0;

	for (i = 0; i < num_cores; ++i) {
		int32_t val;

		format_path(buf, sizeof(buf), CPU_MAX_FREQ, i);
		if (pg_sysfs_read_i32(buf, &val) == 0 && val > freq)
			freq = val;
	}

	return freq;
}
