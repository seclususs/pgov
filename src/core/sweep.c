// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#define _GNU_SOURCE
#include "pg/sweep.h"
#include "fs.h"
#include "pg/config.h"
#include "pg/log.h"
#include "task.h"
#include "sensor.h"
#include "paths.h"
#include "compiler.h"
#include "psi.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

struct sweep_ctx {
	struct pg_context *pg_ctx;
	uint64_t now_sec;
	int nr_files;
	bool intr;
};

#define AGE_LIMIT 86400
#define MAX_DEPTH 12
#define UCLAMP_UNRESTRICTED 1024

#define DIR_FLAGS (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME)

static const char *const TEMP_PATHS[] = {
	"/data/media/0/DCIM/.thumbnails", "/data/media/0/Movies/.thumbnails",
	"/data/media/0/Pictures/.thumbnails"
};

static const char *const APP_ROOTS[] = { "/data/data",
					 "/data/media/0/Android/data" };

static bool sweep_chk_intr(struct sweep_ctx *sctx)
{
	if (UNLIKELY(sctx->intr))
		return true;

	if ((++sctx->nr_files & 1023) != 0)
		return false;

	int32_t bl = 0;
	pg_sensor_read_bl(&sctx->pg_ctx->bl_sensor, &bl);
	if (bl > 0) {
		sctx->intr = true;
		return true;
	}

	q16_t avg10 = 0;
	if (pg_psi_read_raw(PG_PATH_PSI_CPU, &avg10) == 0) {
		if (avg10 > INT_TO_Q16(3)) {
			sctx->intr = true;
			return true;
		}
	}

	return false;
}

static enum pg_fs_act sweep_cb(int dfd, const char *RESTRICT name, uint8_t type,
			       int depth, void *RESTRICT user)
{
	UNUSED(depth);
	struct sweep_ctx *sctx = user;
	struct stat st;

	if (sweep_chk_intr(sctx))
		return FS_STOP;

	if (type == DT_DIR)
		return FS_KEEP;

	if (fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0)
		return FS_KEEP;

	uint64_t s_mtime = (st.st_mtime < 0) ? 0 : (uint64_t)st.st_mtime;
	if (!S_ISREG(st.st_mode) || sctx->now_sec <= s_mtime)
		return FS_KEEP;

	uint64_t age = sctx->now_sec - s_mtime;
	if (age > AGE_LIMIT)
		return FS_DELETE;

	return FS_KEEP;
}

static void sweep_package(int root_fd, const char *RESTRICT pkg_name,
			  struct sweep_ctx *RESTRICT sctx)
{
	int pkg_fd = openat(root_fd, pkg_name, DIR_FLAGS);
	if (pkg_fd < 0)
		return;

	int cache_fd = openat(pkg_fd, "cache", DIR_FLAGS);
	if (cache_fd >= 0) {
		pg_fs_walk_fd(cache_fd, sweep_cb, sctx, MAX_DEPTH);
		close(cache_fd);
	}

	if (sctx->intr)
		goto out_close;

	int code_fd = openat(pkg_fd, "code_cache", DIR_FLAGS);
	if (code_fd >= 0) {
		pg_fs_walk_fd(code_fd, sweep_cb, sctx, MAX_DEPTH);
		close(code_fd);
	}

out_close:
	close(pkg_fd);
}

static void sweep_process_dirent(int root_fd, struct linux_dirent64 *d,
				 struct sweep_ctx *sctx)
{
	uint8_t dtype = d->d_type;

	if (sweep_chk_intr(sctx))
		return;

	if (d->d_name[0] == '.')
		return;

	if (UNLIKELY(dtype == DT_UNKNOWN)) {
		struct stat st;
		int ret;
		ret = fstatat(root_fd, d->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret == 0 && S_ISDIR(st.st_mode))
			dtype = DT_DIR;
	}

	if (dtype == DT_DIR)
		sweep_package(root_fd, d->d_name, sctx);
}

static void sweep_app_data(int root_fd, struct sweep_ctx *sctx)
{
	char buf[32768];
	long nrd;

	while ((nrd = syscall(SYS_getdents64, root_fd, buf, sizeof(buf))) > 0) {
		long bpos = 0;

		while (bpos < nrd) {
			struct linux_dirent64 *d = (void *)(buf + bpos);
			bpos += d->d_reclen;

			if (UNLIKELY(d->d_reclen == 0))
				break;

			sweep_process_dirent(root_fd, d, sctx);
			if (UNLIKELY(sctx->intr))
				return;
		}
	}
}

void pg_sweep_run(struct pg_context *ctx)
{
	struct sweep_ctx sctx = { 0 };
	sctx.pg_ctx = ctx;
	sctx.intr = false;
	sctx.nr_files = 0;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	sctx.now_sec = (uint64_t)ts.tv_sec;
	if (sctx.now_sec < 1767225600ULL)
		return;

	if (sweep_chk_intr(&sctx))
		return;

	pg_task_set_uclamp(UCLAMP_UNRESTRICTED);

	for (size_t i = 0; i < ARRAY_SIZE(APP_ROOTS); ++i) {
		if (sctx.intr)
			break;

		int root_fd = open(APP_ROOTS[i], DIR_FLAGS);
		if (root_fd < 0)
			continue;

		sweep_app_data(root_fd, &sctx);
		close(root_fd);
	}

	for (size_t i = 0; i < ARRAY_SIZE(TEMP_PATHS); ++i) {
		if (sctx.intr)
			break;

		pg_fs_walk(TEMP_PATHS[i], sweep_cb, &sctx, MAX_DEPTH);
	}

	pg_task_set_uclamp(PG_UCLAMP_MAX);

	if (sctx.intr)
		LOGD("sweep: cycle preempted, intr=1");
}

#endif // NDK_BUILD
