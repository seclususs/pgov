// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SIGNAL_H
#define PGOV_SIGNAL_H

/*
 * Block termination signals (SIGINT, SIGTERM, SIGHUP) and create a
 * signal file descriptor for epoll integration.
 * Caller owns the returned file descriptor
 *
 * Returns fd on success, or -errno on failure
 */
int signal_init(void);

/* Register signal crash hook (SIGSEGV, SIGFPE, SIGABRT, SIGILL) */
void signal_catch_crash(void (*hook)(void));

void signal_close(int fd);

#endif // PGOV_SIGNAL_H
