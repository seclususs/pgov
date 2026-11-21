// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_EPOLL_H
#define PGOV_EPOLL_H

#include "pgov/state.h"

int epoll_add_trigger(struct context *ctx);
void epoll_rm_trigger(struct context *ctx);

/*
 * Run the blocking reactor loop, dispatching epoll events, timers, and
 * signals to context callbacks until shutdown is requested
 *
 * Returns 0 on clean shutdown, or -errno if epoll creation fails
 */
int epoll_run(struct context *ctx);

#endif // PGOV_EPOLL_H
