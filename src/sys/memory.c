// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "memory.h"
#include "pg/log.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MCL_ONFAULT
#define MCL_ONFAULT 4
#endif

#define OOM_SCORE_ADJ "/proc/self/oom_score_adj"
#define OOM_SCORE_MIN "-1000\n"

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

	LOGE("memory: failed to lock memory err=%d, gc latency risk", errno);
}

void pg_memory_shield(void)
{
	int fd = open(OOM_SCORE_ADJ, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		LOGW("memory: failed to open %s", OOM_SCORE_ADJ);
		return;
	}

	ssize_t sz;
	do {
		sz = write(fd, OOM_SCORE_MIN, sizeof(OOM_SCORE_MIN) - 1);
	} while (sz < 0 && errno == EINTR);

	if (sz < 0)
		LOGE("memory: failed to write oom protection score");
	else
		LOGD("memory: lmk shield active");

	close(fd);
}
