// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_FS_H
#define PGOV_FS_H

#include "compiler.h"
#include <stdint.h>
#include <stdbool.h>

#if defined(NDK_BUILD)

struct linux_dirent64 {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

enum pg_fs_act { FS_KEEP = 0, FS_DELETE, FS_STOP };

typedef enum pg_fs_act (*pg_fs_cb)(int dfd, const char *RESTRICT name,
				   uint8_t type, int depth, void *RESTRICT ctx);

int pg_fs_walk_fd(int dfd, pg_fs_cb cb, void *ctx, int max_depth);

int pg_fs_walk(const char *RESTRICT path, pg_fs_cb cb, void *RESTRICT ctx,
	       int max_depth);

#endif // NDK_BUILD

#endif // PGOV_FS_H
