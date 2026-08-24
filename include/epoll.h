// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_EPOLL_H
#define PGOV_EPOLL_H

#include "pg/state.h"

int pg_epoll_add_trg(struct pg_context *ctx);
void pg_epoll_rm_trg(struct pg_context *ctx);

/**
 * @brief Starts the blocking event reactor loop, dispatching events to context callbacks.
 *
 * @param ctx Global state container providing signal and trigger file descriptors.
 *
 * @note MEMORY OWNERSHIP & IMPLICIT CONTRACTS:
 *       - Creates and strictly owns a new epoll file descriptor internally.
 *         Guarantees clean closure (`close(ctx->epoll_fd)`) upon exiting the reactor.
 *       - Blocks the thread on `epoll_wait` until an I/O event, timeout (`ctx->next_wake`),
 *         or signal interruption (`EINTR`) occurs.
 *       - On `EINTR`, mathematically calculates the elapsed monotonic time to adjust
 *         the remaining sleep duration, preventing runaway wakes.
 *
 * @return 0 on a clean shutdown, or a negative POSIX error code if epoll creation fails.
 */
int pg_epoll_run(struct pg_context *ctx);

#endif // PGOV_EPOLL_H
