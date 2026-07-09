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

void pg_sysfs_cache_init(struct pg_sysfs_cache *RESTRICT cache,
			 const char *RESTRICT path);

void pg_sysfs_cache_cleanup(struct pg_sysfs_cache *cache);

int pg_sysfs_read_i32(const char *path, int32_t *out_val);

int pg_sysfs_write_strm(int fd, uint64_t value);

void pg_sysfs_update(struct pg_sysfs_cache *RESTRICT cache, uint64_t value,
		     bool force, const struct pg_sysfs_chk *RESTRICT strat);

#endif // PGOV_SYSFS_H
