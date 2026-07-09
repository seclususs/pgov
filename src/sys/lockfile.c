// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "lockfile.h"
#include "pg/log.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int pg_lockfile_acquire(const char *path)
{
	int err;
	int fd = open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (fd < 0) {
		err = errno;
		LOGE("lockfile: failed to open lockfile %s err=%d", path, err);
		return -err;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		LOGE("lockfile: another process is already running");
		close(fd);
		return -EAGAIN;
	}

	return 0;
}
