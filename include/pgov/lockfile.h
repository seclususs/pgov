// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_LOCKFILE_H
#define PGOV_LOCKFILE_H

/*
 * Acquire an exclusive flock() lock on the single-instance lock file.
 * Caller owns the returned fd and must close it via lockfile_close()
 *
 * Returns fd on success, or a negative POSIX error code on failure
 */
int lockfile_init(const char *path);
void lockfile_close(int fd);

#endif // PGOV_LOCKFILE_H
