// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#ifndef PGOV_CONF_H
#define PGOV_CONF_H

typedef int (*conf_callback)(const char *key, const char *val);

int conf_parse(const char *path, conf_callback cb);

#endif // PGOV_CONF_H

#endif // NDK_BUILD
