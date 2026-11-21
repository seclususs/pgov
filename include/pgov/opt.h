// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#ifndef PGOV_OPT_H
#define PGOV_OPT_H

/*
 * Load optional module overrides from pgovd.conf and apply custom
 * node writes and Android property overrides
 *
 * Returns 0 on success (including missing file), or a negative error code
 */
int opt_init(void);

/* Restore overridden Android system properties */
void opt_exit(void);

#endif // PGOV_OPT_H

#endif // NDK_BUILD
