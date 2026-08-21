// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_GOV_H
#define PG_GOV_H

#include "state.h"

int pg_gov_init(struct pg_context *ctx);

/**
 * @brief Executes a single iteration of the governor's control loop.
 *
 * @pre The context `ctx` must have been successfully initialized via pg_gov_init()
 *      and its file descriptors (e.g., PSI triggers) must be valid and actively monitored.
 *
 * @param ctx Mutable reference to the global governor state machine.
 *
 * @note SIDE EFFECTS & SYSTEM IMPACT:
 *       - Performs blocking/non-blocking reads on hardware sensors (battery, thermal, backlight).
 *       - Emits pwrite() syscalls directly to kernel sysfs nodes if scheduler parameters drift.
 *       - Modifies `ctx->next_wake` to dictate the caller's subsequent sleep/poll duration.
 *       - In cases of system pressure read failure, it may automatically close and reopen
 *         epoll triggers for PSI recovery.
 *
 * @warning This function is strictly designed for a single-threaded execution context.
 *          Concurrent invocation leads to undefined behavior and race conditions.
 */
void pg_gov_process(struct pg_context *ctx);

#endif // PG_GOV_H
