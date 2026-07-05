// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_EPOLL_H
#define PGOV_EPOLL_H

#include "pg/state.h"

int pg_epoll_add_trg(struct pg_context *ctx);
void pg_epoll_rm_trg(struct pg_context *ctx);
int pg_epoll_run(struct pg_context *ctx);

#endif // PGOV_EPOLL_H
