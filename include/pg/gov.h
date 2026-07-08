// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_GOV_H
#define PG_GOV_H

#include "state.h"

int pg_gov_init(struct pg_context *ctx);
void pg_gov_process(struct pg_context *ctx);

#endif // PG_GOV_H
