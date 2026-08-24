// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_DAEMON_H
#define PGOV_DAEMON_H

/**
 * @brief Bootstraps the daemon lifecycle, system OS limits, and the event reactor.
 *
 * @note ARCHITECTURAL LIFECYCLE & CONTRACTS:
 *       - **Initialization Phase**: Executes a rigid sequence of privilege checks,
 *         PID lockfile creation, OS shielding (`mlockall`, `rlimit`), and topology discovery.
 *       - **Reactor Phase**: Transfers execution ownership to the `epoll` blocking loop.
 *       - **Teardown Phase**: Features a unified `cleanup:` jump label that guarantees 
 *         deterministic destruction of all acquired file descriptors
 *         (sysfs caches, signalfd, PSI triggers) regardless of exit status.
 *       - Failing to complete initialization aborts safely without leaking allocated resources.
 *
 * @return 0 upon successful, clean daemon shutdown, or a negative POSIX error code
 *         if the bootstrap sequence or reactor encounters an unrecoverable fault.
 */
int pg_daemon_init(void);

#endif // PGOV_DAEMON_H
