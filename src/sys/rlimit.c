// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "rlimit.h"
#include "pg/log.h"
#include <errno.h>
#include <sys/resource.h>

int pg_rlimit_set_max_fd(void)
{
	struct rlimit rl_fd = { 0 };
	if (getrlimit(RLIMIT_NOFILE, &rl_fd) != 0)
		return -errno;

	rl_fd.rlim_cur = rl_fd.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rl_fd) != 0) {
		int err = errno;
		LOGW("rlimit: failed to maximize fd limit err=%d", err);
		return -err;
	}

	LOGD("rlimit: fd limit maximized to %lu",
	     (unsigned long)rl_fd.rlim_cur);
	return 0;
}

int pg_rlimit_set_stack(size_t bytes)
{
	struct rlimit rl_stack = { 0 };
	if (getrlimit(RLIMIT_STACK, &rl_stack) != 0)
		return -errno;

	rlim_t target = (rlim_t)bytes;
	if (rl_stack.rlim_max != RLIM_INFINITY && target > rl_stack.rlim_max)
		target = rl_stack.rlim_max;

	rl_stack.rlim_cur = target;
	if (setrlimit(RLIMIT_STACK, &rl_stack) != 0) {
		int err = errno;
		LOGW("rlimit: failed to set stack limit err=%d", err);
		return -err;
	}

	LOGD("rlimit: stack bounded to %lu bytes",
	     (unsigned long)rl_stack.rlim_cur);
	return 0;
}
