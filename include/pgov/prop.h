// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PROP_H
#define PGOV_PROP_H

#include "pgov/compiler.h"
#include <stdbool.h>
#include <sys/system_properties.h>

#if defined(NDK_BUILD)

#define PROP_KEY_MAX 256

struct prop_state {
	char key[PROP_KEY_MAX];
	char val[PROP_VALUE_MAX];
	bool modified;
};

void prop_init(struct prop_state *RESTRICT state, const char *RESTRICT key);

int prop_set(struct prop_state *RESTRICT state, const char *RESTRICT val);

int prop_reset(struct prop_state *state);

void prop_cleanup(struct prop_state *state);

#endif // NDK_BUILD

bool prop_wait_boot(void);

#endif // PGOV_PROP_H
