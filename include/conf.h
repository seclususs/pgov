// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#ifndef PGOV_CONF_H
#define PGOV_CONF_H

#include "compiler.h"

typedef int (*pg_conf_cb)(const char *RESTRICT key, const char *RESTRICT val);

int pg_conf_parse(const char *RESTRICT path, pg_conf_cb cb);

#endif // PGOV_CONF_H

#endif // NDK_BUILD
