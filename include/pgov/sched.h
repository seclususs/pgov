// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SCHED_H
#define PGOV_SCHED_H

#include "pgov/state.h"

int sched_init(struct context *ctx);

/*
 * Snapshot active base values for crash-time recovery
 */
void sched_snapshot(const struct context *ctx);

/*
 * Async-signal-safe recovery hook restoring snapshot base values via pwrite
 */
void sched_restore(void);

/*
 * Restore tunables, release open descriptors, and cleanup sched node caches
 */
void sched_cleanup(struct context *ctx);

#endif // PGOV_SCHED_H
