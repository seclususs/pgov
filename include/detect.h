// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_DETECT_H
#define PGOV_DETECT_H

#include <stdbool.h>

bool pg_detect_cpu_psi(void);
int pg_detect_privilege(void);

#endif // PGOV_DETECT_H
