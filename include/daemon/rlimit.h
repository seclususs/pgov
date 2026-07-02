// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_RLIMIT_H
#define PASCAL_GOV_DAEMON_RLIMIT_H

#include <stddef.h>

int pascal_gov_rlimit_set_max_fd(void);
int pascal_gov_rlimit_set_stack(size_t bytes);

#endif // PASCAL_GOV_DAEMON_RLIMIT_H
