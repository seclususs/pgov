// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_SWEEP_H
#define PG_SWEEP_H

#include "pg/state.h"

#if defined(NDK_BUILD)
void pg_sweep_run(struct pg_context *ctx);
#endif // NDK_BUILD

#endif // PG_SWEEP_H
