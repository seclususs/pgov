// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_GOV_H
#define PGOV_GOV_H

#include "pgov/state.h"

/*
 * Execute one tick of the governor control loop by polling sensors,
 * computing demand, updating kernel nodes, and set next wake timeout
 *
 * Must be called from the single reactor thread
 */
void gov_run(struct context *ctx);

#endif // PGOV_GOV_H
