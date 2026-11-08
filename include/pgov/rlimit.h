// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_RLIMIT_H
#define PGOV_RLIMIT_H

#include <stddef.h>

int pg_rlimit_set_max_fd(void);
int pg_rlimit_set_stack(size_t bytes);

#endif // PGOV_RLIMIT_H
