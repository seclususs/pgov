// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_FS_H
#define PGOV_FS_H

#include <stdbool.h>
#include <stdint.h>

#if defined(NDK_BUILD)

struct linux_dirent64 {
	uint64_t d_ino;
	int64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

enum fs_action {
	FS_KEEP = 0,
	FS_DELETE,
	FS_STOP,
};

typedef enum fs_action (*fs_callback)(int dfd, const char *name, uint8_t type,
				      int depth, void *fsctx);

/*
 * Recursively walk directory tree up to max depth, invoking callback on
 * each entry. The callback returns FS_KEEP, FS_DELETE, or FS_STOP
 *
 * Returns 0 on success, or -errno on failure
 */
int fs_walk_fd(int dfd, fs_callback cb, void *fsctx, int max);

int fs_walk(const char *path, fs_callback cb, void *fsctx, int max);

#endif // NDK_BUILD

#endif // PGOV_FS_H
