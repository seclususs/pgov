// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TOPO_H
#define PGOV_TOPO_H

#include <stdint.h>

int pg_topo_set_little_core(void);
int32_t pg_topo_get_core_count(void);
int32_t pg_topo_get_max_freq_khz(void);

#endif // PGOV_TOPO_H
