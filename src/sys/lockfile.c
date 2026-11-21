// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/lockfile.h"
#include "pgov/compiler.h"
#include "pgov/log.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int lockfile_init(const char *path)
{
	int err;
	int fd;

	if (UNLIKELY(!path))
		return -EINVAL;

	fd = open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (fd < 0) {
		err = errno;
		LOGE("lockfile: failed to open lockfile %s err=%d", path, err);
		return -err;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		LOGE("lockfile: another process is already running");
		err = -EAGAIN;
		goto err_close;
	}

	return fd;

err_close:
	close(fd);
	return err;
}

void lockfile_close(int fd)
{
	if (fd >= 0)
		close(fd);
}
