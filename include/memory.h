// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_MEMORY_H
#define PGOV_MEMORY_H

/**
 * @brief Locks the current process's virtual address space into physical RAM.
 *
 * @note SIDE EFFECTS & SYSTEM IMPACT:
 *       - Issues the `mlockall` syscall, preventing the OS from swapping out
 *         the process pages to secondary storage or zRAM.
 *       - Eliminates major page faults during runtime, guaranteeing deterministic
 *         execution latency for the critical governor loop.
 *       - Prioritizes `MCL_ONFAULT` (lazy locking) to minimize initial footprint,
 *         falling back to immediate aggressive locking if the kernel lacks support.
 */
void pg_memory_lock(void);

void pg_memory_shield(void);

#endif // PGOV_MEMORY_H
