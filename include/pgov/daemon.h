// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_DAEMON_H
#define PGOV_DAEMON_H

/*
 * Bootstrap the daemon process, acquire the single-instance lockfile,
 * configure system limits, discover topology, and run the event reactor
 *
 * Returns 0 on clean shutdown, or a negative POSIX error code on failure
 */
int daemon_init(void);

#endif // PGOV_DAEMON_H
