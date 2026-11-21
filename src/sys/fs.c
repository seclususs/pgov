// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/fs.h"
#include "pgov/compiler.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(NDK_BUILD)

#define DIR_FLAGS (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME)

static int walk(int dfd, fs_callback cb, void *fsctx, int max, int depth);

static int process_entry(int dfd, const struct linux_dirent64 *dir,
			 fs_callback cb, void *fsctx, int max, int depth)
{
	struct stat st;
	enum fs_action action;
	uint8_t dtype;
	int flags;
	int ret;
	int sfd;

	if (dir->d_name[0] == '.' &&
	    (dir->d_name[1] == '\0' ||
	     (dir->d_name[1] == '.' && dir->d_name[2] == '\0')))
		return 0;

	dtype = dir->d_type;
	if (dtype == DT_LNK)
		return 0;

	if (UNLIKELY(dtype == DT_UNKNOWN)) {
		ret = fstatat(dfd, dir->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret == 0 && S_ISDIR(st.st_mode))
			dtype = DT_DIR;
	}

	action = cb(dfd, dir->d_name, dtype, depth, fsctx);
	if (action == FS_STOP)
		return -ECANCELED;

	if (action == FS_DELETE) {
		flags = (dtype == DT_DIR) ? AT_REMOVEDIR : 0;
		unlinkat(dfd, dir->d_name, flags);
		return 0;
	}

	if (dtype == DT_DIR) {
		sfd = openat(dfd, dir->d_name, DIR_FLAGS);
		if (sfd < 0)
			return 0;

		ret = walk(sfd, cb, fsctx, max, depth + 1);
		close(sfd);
		if (ret != 0)
			return ret;
	}

	return 0;
}

static int walk(int dfd, fs_callback cb, void *fsctx, int max, int depth)
{
	char buf[8192];
	const struct linux_dirent64 *dir;
	long pos;
	long res;
	int ret;

	if (depth > max)
		return 0;

	for (;;) {
		do {
			res = syscall(SYS_getdents64, dfd, buf, sizeof(buf));
		} while (res < 0 && errno == EINTR);

		if (res <= 0)
			break;

		pos = 0;
		while (pos < res) {
			dir = (void *)(buf + pos);
			pos += dir->d_reclen;

			if (UNLIKELY(dir->d_reclen == 0))
				break;

			ret = process_entry(dfd, dir, cb, fsctx, max, depth);
			if (ret != 0)
				return ret;
		}
	}

	return (res < 0) ? -errno : 0;
}

int fs_walk_fd(int dfd, fs_callback cb, void *fsctx, int max)
{
	if (dfd < 0)
		return -EBADF;

	if (UNLIKELY(!cb || max < 0))
		return -EINVAL;

	return walk(dfd, cb, fsctx, max, 0);
}

int fs_walk(const char *path, fs_callback cb, void *fsctx, int max)
{
	int fd;
	int ret;

	if (UNLIKELY(!path || !cb || max < 0))
		return -EINVAL;

	fd = open(path, DIR_FLAGS);
	if (fd < 0)
		return -errno;

	ret = fs_walk_fd(fd, cb, fsctx, max);
	close(fd);

	return ret;
}

#endif // NDK_BUILD
