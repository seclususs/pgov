// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/memory.h"
#include "pgov/log.h"
#include <errno.h>
#include <sys/mman.h>

#ifndef MCL_ONFAULT
#define MCL_ONFAULT 4
#endif

void pg_memory_lock(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) == 0) {
		LOGD("memory: smart ram locking enabled");
		return;
	}

	LOGW("memory: on-fault locking unsupported");

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {
		LOGD("memory: aggressive ram locking enabled");
		return;
	}

	LOGE("memory: failed to lock mem err=%d", errno);
}
