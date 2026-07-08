// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "fs.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(NDK_BUILD)

#define DIR_FLAGS (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME)

static int walk(int dfd, pg_fs_cb cb, void *ctx, int max_depth, int cur_depth)
{
	char buf[8192];

	if (cur_depth > max_depth)
		return 0;

	while (1) {
		long nread = syscall(SYS_getdents64, dfd, buf, sizeof(buf));
		long bpos = 0;

		if (nread <= 0)
			return (nread == -1) ? -1 : 0;

		while (bpos < nread) {
			struct linux_dirent64 *d = (void *)(buf + bpos);
			enum pg_fs_act act;
			uint8_t dtype = d->d_type;

			bpos += d->d_reclen;

			if (d->d_name[0] == '.') {
				if (d->d_name[1] == '\0')
					continue;

				if (d->d_name[1] == '.' && d->d_name[2] == '\0')
					continue;
			}

			if (dtype == DT_LNK)
				continue;

			if (UNLIKELY(dtype == DT_UNKNOWN)) {
				struct stat st;
				if (fstatat(dfd, d->d_name, &st,
					    AT_SYMLINK_NOFOLLOW) == 0) {
					if (S_ISDIR(st.st_mode))
						dtype = DT_DIR;
				}
			}

			act = cb(dfd, d->d_name, dtype, cur_depth, ctx);

			if (act == FS_STOP)
				return 1;

			if (act == FS_DELETE) {
				if (dtype == DT_DIR)
					unlinkat(dfd, d->d_name, AT_REMOVEDIR);
				else
					unlinkat(dfd, d->d_name, 0);

				continue;
			}

			if (dtype == DT_DIR) {
				int sfd = openat(dfd, d->d_name, DIR_FLAGS);
				if (sfd >= 0) {
					int stop = walk(sfd, cb, ctx, max_depth,
							cur_depth + 1);
					close(sfd);
					if (stop)
						return 1;
				}
			}
		}
	}

	return 0;
}

int pg_fs_walk_fd(int dfd, pg_fs_cb cb, void *ctx, int max_depth)
{
	if (dirfd < 0)
		return -1;

	return walk(dfd, cb, ctx, max_depth, 0) ? 1 : 0;
}

int pg_fs_walk(const char *RESTRICT path, pg_fs_cb cb, void *RESTRICT ctx,
	       int max_depth)
{
	int fd = open(path, DIR_FLAGS);
	if (fd < 0)
		return -1;

	int ret = pg_fs_walk_fd(fd, cb, ctx, max_depth);
	close(fd);

	return ret;
}

#endif // NDK_BUILD
