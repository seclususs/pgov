// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_NODE_H
#define PGOV_NODE_H

#include "pgov/compiler.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

enum node_check_type {
	NODE_CHECK_ABSOLUTE = 0,
	NODE_CHECK_RELATIVE,
	NODE_CHECK_STRICT,
};

struct node_check {
	enum node_check_type type;
	uint64_t thresh;
};

struct node_cache {
	uint64_t last;
	uint64_t base;
	int fd;
	uint32_t retry_ms;
	uint8_t fails;
	bool has_base;
	bool active;
};

/*
 * Probe candidate paths sequentially to open a persistent node fd.
 * If opened, cache->fd takes ownership and must be released via
 * node_cache_cleanup()
 *
 * Returns 1 on success, 0 if no candidate opened, or -EINVAL on bad arguments
 */
int node_cache_init(struct node_cache *RESTRICT cache,
		    const char *const *RESTRICT paths);

void node_restore(struct node_cache *cache);

void node_cache_cleanup(struct node_cache *cache);

/*
 * Read and parse a 32-bit signed integer from node path
 *
 * Returns 0 on success, or -errno on failure
 */
int node_read_i32(const char *path, int32_t *out_val);

/*
 * Read and parse a 64-bit unsigned integer from node path
 *
 * Returns 0 on success, or -errno on failure
 */
int node_read_u64(const char *path, uint64_t *out_val);

/*
 * Read string from path into buf up to len - 1 bytes and strip trailing
 * newline or carriage return
 *
 * Returns number of bytes read on success, or -1 on error
 */
ssize_t node_read_str(const char *path, char *buf, size_t len);

/*
 * Format val as ASCII decimal and write via pwrite() at offset 0
 *
 * Returns 0 on success, or a negative error code (-EBADF, -EIO, -errno)
 */
int node_write_stream(int fd, uint64_t val);

/*
 * Write null-terminated string to path with trailing newline if absent
 *
 * Returns 0 on success, or a negative error code (-EINVAL, -EIO, -errno)
 */
int node_write(const char *path, const char *val);

/*
 * Update cached node value according to threshold check with exponential
 * backoff retry on failure
 */
void node_update(struct node_cache *cache, uint64_t val, bool force,
		 const struct node_check *check);

#endif // PGOV_NODE_H
