// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "daemon/rlimit.h"
#include "daemon/logger.h"
#include <sys/resource.h>

#define STACK_SIZE_BYTES (512UL * 1024UL)

void pascal_gov_rlimit_expand(void)
{
	struct rlimit rl_fd = {0};

	if (getrlimit(RLIMIT_NOFILE, &rl_fd) == 0) {
		rl_fd.rlim_cur = rl_fd.rlim_max;

		if (setrlimit(RLIMIT_NOFILE, &rl_fd) == 0)
			LOGD("rlimit: fd limit maximized to %lu",
			     (unsigned long)rl_fd.rlim_cur);
		else
			LOGW("rlimit: failed to maximize fd limit");
	}

	struct rlimit rl_stack = {0};

	if (getrlimit(RLIMIT_STACK, &rl_stack) == 0) {
		rlim_t target = STACK_SIZE_BYTES;

		if (rl_stack.rlim_max != RLIM_INFINITY &&
		    target > rl_stack.rlim_max)
			target = rl_stack.rlim_max;

		rl_stack.rlim_cur = target;

		if (setrlimit(RLIMIT_STACK, &rl_stack) == 0)
			LOGD("rlimit: stack bounded to %lu bytes",
			     (unsigned long)rl_stack.rlim_cur);
	}
}
