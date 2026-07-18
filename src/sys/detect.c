// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "detect.h"
#include "pg/log.h"
#include "paths.h"
#include <errno.h> // IWYU pragma: keep
#include <unistd.h>

bool pg_detect_cpu_psi(void)
{
	if (access(PG_PATH_PSI_CPU, R_OK | W_OK) == 0)
		return true;

	LOGE("detect: cpu psi node missing or access denied");
	return false;
}

int pg_detect_privilege(void)
{
	if (getuid() != 0)
		return -EPERM;

	return 0;
}

int32_t pg_detect_kernel_hz(void)
{
	long hz = sysconf(_SC_CLK_TCK);
	if (hz <= 0)
		return 100;

	return (int32_t)hz;
}
