// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_GOV_H
#define PGOV_GOV_H

#include "pgov/state.h"

/**
 * @brief Executes a single iteration of the governor's control loop.
 *
 * @param ctx Mutable reference to the global governor state machine.
 *
 * @note SIDE EFFECTS & SYSTEM IMPACT:
 *       - Performs blocking/non-blocking reads on hardware sensors (battery, thermal, backlight).
 *       - Emits pwrite() syscalls directly to kernel nodes if scheduler parameters drift.
 *       - Modifies `ctx->next_wake` to dictate the caller's subsequent sleep/poll duration.
 *       - In cases of system pressure read failure, it may automatically close and reopen
 *         epoll triggers for PSI recovery.
 *       - On `NDK_BUILD`, while the display is suspended and the sweep interval has elapsed,
 *         it may invoke `sweep_run()`, which deletes stale files from application cache
 *         directories.
 *
 * @warning This function is strictly designed for a single-threaded execution context.
 *          Concurrent invocation leads to undefined behavior and race conditions.
 */
void gov_run(struct context *ctx);

#endif // PGOV_GOV_H
