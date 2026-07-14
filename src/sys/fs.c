// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "fs.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(NDK_BUILD)

#define DIR_FLAGS (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME)

static int walk(int dfd, pg_fs_cb cb, void *ctx, int max_depth, int cur_depth);

static int dirent(int dfd, struct linux_dirent64 *d, pg_fs_cb cb, void *ctx,
		  int max_depth, int cur_depth)
{
	uint8_t dtype = d->d_type;

	if (d->d_name[0] == '.' &&
	    (d->d_name[1] == '\0' ||
	     (d->d_name[1] == '.' && d->d_name[2] == '\0')))
		return 0;

	if (dtype == DT_LNK)
		return 0;

	if (UNLIKELY(dtype == DT_UNKNOWN)) {
		struct stat st;
		int ret;

		ret = fstatat(dfd, d->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret == 0 && S_ISDIR(st.st_mode))
			dtype = DT_DIR;
	}

	enum pg_fs_act act = cb(dfd, d->d_name, dtype, cur_depth, ctx);

	if (act == FS_STOP)
		return 1;

	if (act == FS_DELETE) {
		int flgs = (dtype == DT_DIR) ? AT_REMOVEDIR : 0;
		unlinkat(dfd, d->d_name, flgs);
		return 0;
	}

	if (dtype == DT_DIR) {
		int sfd = openat(dfd, d->d_name, DIR_FLAGS);
		if (sfd < 0)
			return 0;

		int stop = walk(sfd, cb, ctx, max_depth, cur_depth + 1);

		close(sfd);

		if (stop)
			return stop;
	}

	return 0;
}

static int walk(int dfd, pg_fs_cb cb, void *ctx, int max_depth, int cur_depth)
{
	char buf[8192];
	long nread;

	if (cur_depth > max_depth)
		return 0;

	while ((nread = syscall(SYS_getdents64, dfd, buf, sizeof(buf))) > 0) {
		long bpos = 0;

		while (bpos < nread) {
			struct linux_dirent64 *d = (void *)(buf + bpos);
			bpos += d->d_reclen;

			if (UNLIKELY(d->d_reclen == 0))
				break;

			int ret = dirent(dfd, d, cb, ctx, max_depth, cur_depth);
			if (ret)
				return ret;
		}
	}

	return (nread < 0) ? -errno : 0;
}

int pg_fs_walk_fd(int dfd, pg_fs_cb cb, void *ctx, int max_depth)
{
	if (dfd < 0)
		return -EBADF;

	return walk(dfd, cb, ctx, max_depth, 0);
}

int pg_fs_walk(const char *RESTRICT path, pg_fs_cb cb, void *RESTRICT ctx,
	       int max_depth)
{
	int fd = open(path, DIR_FLAGS);
	if (fd < 0)
		return -errno;

	int ret = pg_fs_walk_fd(fd, cb, ctx, max_depth);
	close(fd);

	return ret;
}

#endif // NDK_BUILD
