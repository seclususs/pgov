// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TOPO_H
#define PGOV_TOPO_H

#include "pgov/compiler.h"
#include <stdint.h>

/*
 * Bind calling thread affinity to efficiency (LITTLE) CPU cores via
 * sched_setaffinity(). To all cores if little cores cannot be isolated
 *
 * Returns 0 on success, or a negative POSIX error code (-EINVAL, -errno)
 */
int topo_set_little_core(void);

int32_t topo_get_core_count(void);
int32_t topo_get_maxfrq_khz(void);

/*
 * Read aggregate and min/max cpu_capacity values across configured cores
 *
 * Returns 0 on success, or a negative error code (-EINVAL, -ENODATA)
 */
int topo_get_cpu_capacity(int32_t *RESTRICT total,
			  int32_t *RESTRICT min_capacity,
			  int32_t *RESTRICT max_capacity);

#endif // PGOV_TOPO_H
