// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_NODE_H
#define PGOV_NODE_H

#include "compiler.h"
#include <stdbool.h>
#include <stdint.h>

enum pg_node_chk_type {
	PG_CHK_ABS,
	PG_CHK_REL,
	PG_CHK_STRICT,
};

struct pg_node_chk {
	enum pg_node_chk_type type;
	uint64_t thresh;
};

struct pg_node_cache {
	uint64_t last;
	int fd;
	uint32_t base;
	uint32_t retry_ms;
	uint8_t fails;
	bool has_base;
	bool active;
};

/**
 * @brief Acquires a persistent file descriptor from candidate node paths.
 *
 * @param cache Pointer to an uninitialized cache structure. Owned by caller.
 * @param paths NULL-terminated array of candidate paths. Read-only borrow.
 *
 * @note SIDE EFFECTS & MEMORY OWNERSHIP:
 *       - Probes candidate paths sequentially with O_WRONLY | O_CLOEXEC flags.
 *       - If successful, `cache->fd` takes ownership of the open description.
 *         The caller must eventually release it via pg_node_cache_cleanup().
 *       - Fails gracefully: If no candidate opens, flags `cache->active = false`
 *         and downstream updates become zero-cost no-ops.
 *
 * @return 1 if a candidate node is opened and activated, 0 if none opened,
 *         or a negative POSIX error code (-EINVAL) on invalid arguments.
 */
int pg_node_cache_init(struct pg_node_cache *RESTRICT cache,
		       const char *const *RESTRICT paths);

void pg_node_restore(struct pg_node_cache *cache);

void pg_node_cache_cleanup(struct pg_node_cache *cache);

int pg_node_read_i32(const char *path, int32_t *out_val);

/**
 * @brief Flushes a 64-bit unsigned integer to a node using a positional write.
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
int pg_node_write_strm(int fd, uint64_t value);

int pg_node_write(const char *path, const char *val);

void pg_node_update(struct pg_node_cache *cache, uint64_t value, bool force,
		    const struct pg_node_chk *strat);

#endif // PGOV_NODE_H
