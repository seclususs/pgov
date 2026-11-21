// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#ifndef PGOV_CONF_H
#define PGOV_CONF_H

typedef int (*conf_callback)(const char *key, const char *val);

/*
 * Parse key-value configuration file, ignoring comments and invoking cb
 * for each setting. Returns 0 on success, or a negative error code
 */
int conf_parse(const char *path, conf_callback cb);

#endif // PGOV_CONF_H

#endif // NDK_BUILD
