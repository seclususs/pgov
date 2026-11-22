// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/memory.h"
#include "pgov/log.h"
#include <errno.h>
#include <sys/mman.h>

#ifndef MCL_ONFAULT
#define MCL_ONFAULT 4
#endif

void memory_lock(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) == 0) {
		LOGD("memory: smart ram locking enabled");
		return;
	}

	LOGD("memory: on-fault locking unsupported");

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0)
		return;

	LOGE("memory: failed to lock mem err=%d", errno);
}
