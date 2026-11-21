// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_MEMORY_H
#define PGOV_MEMORY_H

/*
 * Lock the process address space into physical memory via mlockall()
 * using MCL_CURRENT | MCL_FUTURE, with MCL_ONFAULT when supported
 */
void memory_lock(void);

#endif // PGOV_MEMORY_H
