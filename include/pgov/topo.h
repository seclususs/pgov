// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_TOPO_H
#define PGOV_TOPO_H

#include "pgov/compiler.h"
#include <stdint.h>

/**
 * @brief Pins the current calling thread to the efficiency (LITTLE) cores.
 *
 * @note SYSTEM SIDE-EFFECTS & CONTRACTS:
 *       - Discovers the hardware topology dynamically by parsing sysfs capacity/frequency
 *         nodes across all logical cores, selecting the cores with the lowest
 *         `cpu_capacity` or, failing that, the lowest `cpuinfo_max_freq`.
 *       - If neither yields a core set that can be applied, it logs a warning and binds
 *         the thread to every core instead, which still counts as success.
 *       - Mutates the thread's scheduling context via the `sched_setaffinity` syscall.
 *       - This function MUST be called during daemon initialization, but BEFORE the
 *         realtime scheduling priorities (e.g., FIFO/RR) are applied.
 *
 * @return 0 on successful affinity binding (including the all-cores fallback), or a
 *         negative POSIX error code (-EINVAL if no cores are reported, otherwise -errno).
 */
int pg_topo_set_little_core(void);

int32_t pg_topo_get_core_count(void);
int32_t pg_topo_get_max_freq_khz(void);

int pg_topo_get_cpu_capacity(int32_t *RESTRICT total,
			     int32_t *RESTRICT min_capacity,
			     int32_t *RESTRICT max_capacity);

#endif // PGOV_TOPO_H
