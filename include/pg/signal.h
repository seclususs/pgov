// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_SIGNAL_H
#define PG_SIGNAL_H

/**
 * @brief Hijacks POSIX signals and converts them into a synchronous file descriptor.
 *
 * @note MEMORY OWNERSHIP & IMPLICIT ARCHITECTURE:
 *       - Issues `sigprocmask(SIG_BLOCK, ...)` globally, disabling standard asynchronous
 *         delivery for SIGINT, SIGTERM, and SIGHUP.
 *       - Transfers the signal delivery mechanism to the returned `signalfd`.
 *       - The caller assumes ownership of the returned file descriptor and MUST
 *         integrate it into the epoll reactor loop.
 *
 * @return A valid file descriptor (owned by the caller) on success, or -errno on failure.
 */
int pg_signal_init(void);

void pg_signal_catch_crash(void);
void pg_signal_close(int fd);

#endif // PG_SIGNAL_H
