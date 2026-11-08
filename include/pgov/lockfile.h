// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_LOCKFILE_H
#define PGOV_LOCKFILE_H

int pg_lockfile_init(const char *path);
void pg_lockfile_close(int fd);

#endif // PGOV_LOCKFILE_H
