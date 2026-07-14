// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PROP_H
#define PGOV_PROP_H

#include "compiler.h"
#include <stdbool.h>
#include <sys/system_properties.h>

#if defined(NDK_BUILD)

struct pg_prop_state {
	char name[PROP_NAME_MAX];
	char val[PROP_VALUE_MAX];
	bool modified;
};

void pg_prop_state_init(struct pg_prop_state *RESTRICT state,
			const char *RESTRICT name);

int pg_prop_set(struct pg_prop_state *RESTRICT state, const char *RESTRICT val);

int pg_prop_reset(struct pg_prop_state *RESTRICT state);

void pg_prop_cleanup(struct pg_prop_state *RESTRICT state);

#endif // NDK_BUILD

bool pg_prop_wait_boot(void);

#endif // PGOV_PROP_H
