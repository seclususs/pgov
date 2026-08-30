// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SYSFS_H
#define PGOV_SYSFS_H

#include "compiler.h"
#include <stdbool.h>
#include <stdint.h>

enum pg_sysfs_chk_type { PG_CHK_ABS, PG_CHK_REL, PG_CHK_STRICT };

struct pg_sysfs_chk {
	enum pg_sysfs_chk_type type;
	uint64_t thresh;
};

struct pg_sysfs_cache {
	int fd;
	uint64_t last;
	bool active;
};

/**
 * @brief Acquires a persistent file descriptor for a target sysfs node.
 *
 * @param cache Pointer to an uninitialized cache structure. Memory is owned by the caller.
 * @param path  Null-terminated absolute path to the sysfs node. Read-only borrow.
 *
 * @note SIDE EFFECTS & MEMORY OWNERSHIP:
 *       - Attempts to acquire a file descriptor with O_WRONLY | O_CLOEXEC flags.
 *       - If successful, `cache->fd` takes explicit ownership of the open file description.
 *         The caller must eventually release it via pg_sysfs_cache_cleanup().
 *       - Fails gracefully: Does NOT return an error code on permission denial; instead,
 *         it flags `cache->active = false`. Downstream updates will become zero-cost no-ops.
 */
void pg_sysfs_cache_init(struct pg_sysfs_cache *RESTRICT cache,
			 const char *RESTRICT path);

void pg_sysfs_cache_cleanup(struct pg_sysfs_cache *cache);

int pg_sysfs_read_i32(const char *path, int32_t *out_val);

/**
 * @brief Flushes a 64-bit unsigned integer to a sysfs node using a positional write.
 *
 * @param fd    A valid file descriptor opened with at least O_WRONLY. Borrowed (not owned).
 * @param value The raw data to be formatted as an ASCII string and written.
 *
 * @note IMPLICIT CONTRACTS & SYSTEM BEHAVIOR:
 *       - Bypasses user-space buffering; issues a direct `pwrite` syscall at absolute offset 0.
 *       - Tolerates `EINTR` by retrying the syscall loop gracefully.
 *       - String formatting allocates a fixed-size 32-byte buffer on the stack, guaranteeing
 *         zero heap allocations (malloc-free hot path).
 *
 * @return 0 on complete write, or a negative POSIX error code (e.g., -EIO, -EBADF).
 */
int pg_sysfs_write_strm(int fd, uint64_t value);

int pg_sysfs_write(const char *RESTRICT path, const char *RESTRICT val);

void pg_sysfs_update(struct pg_sysfs_cache *RESTRICT cache, uint64_t value,
		     bool force, const struct pg_sysfs_chk *RESTRICT strat);

#endif // PGOV_SYSFS_H
