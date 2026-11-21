// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_LOCKFILE_H
#define PGOV_LOCKFILE_H

int lockfile_init(const char *path);
void lockfile_close(int fd);

#endif // PGOV_LOCKFILE_H
