// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#define _GNU_SOURCE
#include "pgov/sweep.h"
#include "pgov/compiler.h"
#include "pgov/fs.h"
#include "pgov/log.h"
#include "pgov/psi.h"
#include "pgov/sensor.h"
#include "pgov/task.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

struct sweep_context {
	struct context *ctx;
	uint64_t now_sec;
	int files_count;
	bool interrupted;
};

#define AGE_LIMIT 43200
#define MAX_PSI 10
#define MAX_DEPTH 12
#define UCLAMP_UNRESTRICTED 1024
#define INTR_MASK 1023

#define DIR_FLAGS (O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NOATIME)

static const char *const TEMP_PATHS[] = {
	"/data/media/0/DCIM/.thumbnails",
	"/data/media/0/Movies/.thumbnails",
	"/data/media/0/Pictures/.thumbnails",
};

static const char *const APP_ROOTS[] = {
	"/data/data",
	"/data/media/0/Android/data",
};

static const char *const CACHE_DIRS[] = {
	"cache",
	"files/cache",
	"files/disk_cache",
};

static const char *const DB_EXTS[] = {
	".db",
	".shm",
	".wal",
	".db-shm",
	".db-wal",
	".sqlite",
	".journal",
	".db-journal",
	".sqlite-shm",
	".sqlite-wal",
	".sqlite-journal",
};

static bool check_database_file(const char *name)
{
	size_t ext_len;
	size_t i;
	size_t len;

	if (UNLIKELY(!name))
		return false;

	len = strlen(name);
	for (i = 0; i < ARRAY_SIZE(DB_EXTS); ++i) {
		ext_len = strlen(DB_EXTS[i]);
		if (len >= ext_len &&
		    strncasecmp(name + len - ext_len, DB_EXTS[i], ext_len) == 0)
			return true;
	}

	return false;
}

static bool check_interrupted(struct sweep_context *sctx)
{
	int32_t backlight = 0;
	q16_t avg10 = 0;

	if (UNLIKELY(!sctx))
		return true;

	if (UNLIKELY(sctx->interrupted))
		return true;

	if ((++sctx->files_count & INTR_MASK) != 0)
		return false;

	sensor_read_backlight(&sctx->ctx->backlight, &backlight);
	if (backlight > 0) {
		sctx->interrupted = true;
		return true;
	}

	if (psi_read_avg10(PSI_CPU, &avg10) == 0) {
		if (avg10 > INT_TO_Q16(MAX_PSI)) {
			sctx->interrupted = true;
			return true;
		}
	}

	return false;
}

static enum fs_action process_entry(int dfd, const char *name, uint8_t type,
				    int depth, void *user)
{
	struct sweep_context *sctx = user;
	struct stat st;
	uint64_t age;
	uint64_t mtime;

	UNUSED(depth);

	if (check_interrupted(sctx))
		return FS_STOP;

	if (type == DT_DIR)
		return FS_KEEP;

	if (check_database_file(name))
		return FS_KEEP;

	if (fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0)
		return FS_KEEP;

	mtime = (st.st_mtime < 0) ? 0 : (uint64_t)st.st_mtime;
	if (!S_ISREG(st.st_mode) || sctx->now_sec <= mtime)
		return FS_KEEP;

	age = sctx->now_sec - mtime;
	if (age > AGE_LIMIT)
		return FS_DELETE;

	return FS_KEEP;
}

static void clean_package(int rfd, const char *pkg_name,
			  struct sweep_context *sctx)
{
	int cfd;
	int pfd;
	size_t i;

	pfd = openat(rfd, pkg_name, DIR_FLAGS);
	if (pfd < 0)
		return;

	for (i = 0; i < ARRAY_SIZE(CACHE_DIRS); ++i) {
		if (sctx->interrupted)
			break;

		cfd = openat(pfd, CACHE_DIRS[i], DIR_FLAGS);
		if (cfd >= 0) {
			fs_walk_fd(cfd, process_entry, sctx, MAX_DEPTH);
			close(cfd);
		}
	}

	close(pfd);
}

static void process_dirent(int rfd, struct linux_dirent64 *dir,
			   struct sweep_context *sctx)
{
	struct stat st;
	uint8_t dtype;
	int ret;

	if (check_interrupted(sctx))
		return;

	if (dir->d_name[0] == '.')
		return;

	dtype = dir->d_type;
	if (UNLIKELY(dtype == DT_UNKNOWN)) {
		ret = fstatat(rfd, dir->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret == 0 && S_ISDIR(st.st_mode))
			dtype = DT_DIR;
	}

	if (dtype == DT_DIR)
		clean_package(rfd, dir->d_name, sctx);
}

static void scan_app_data(int rfd, struct sweep_context *sctx)
{
	char buf[32768];
	struct linux_dirent64 *dir;
	long pos;
	long res;

	for (;;) {
		do {
			res = syscall(SYS_getdents64, rfd, buf, sizeof(buf));
		} while (res < 0 && errno == EINTR);

		if (res <= 0)
			break;

		pos = 0;
		while (pos < res) {
			dir = (void *)(buf + pos);
			pos += dir->d_reclen;

			if (UNLIKELY(dir->d_reclen == 0))
				break;

			process_dirent(rfd, dir, sctx);
			if (UNLIKELY(sctx->interrupted))
				return;
		}
	}
}

void sweep_run(struct context *ctx)
{
	struct sweep_context sctx = { 0 };
	struct timespec ts;
	int rfd;
	size_t i;

	if (UNLIKELY(!ctx))
		return;

	sctx.ctx = ctx;
	sctx.interrupted = false;
	sctx.files_count = INTR_MASK;

	clock_gettime(CLOCK_REALTIME, &ts);
	sctx.now_sec = (uint64_t)ts.tv_sec;
	if (sctx.now_sec < 1767225600ULL)
		return;

	if (check_interrupted(&sctx))
		return;

	task_set_uclamp(UCLAMP_UNRESTRICTED);

	for (i = 0; i < ARRAY_SIZE(APP_ROOTS); ++i) {
		if (sctx.interrupted)
			break;

		rfd = open(APP_ROOTS[i], DIR_FLAGS);
		if (rfd < 0)
			continue;

		scan_app_data(rfd, &sctx);
		close(rfd);
	}

	for (i = 0; i < ARRAY_SIZE(TEMP_PATHS); ++i) {
		if (sctx.interrupted)
			break;

		fs_walk(TEMP_PATHS[i], process_entry, &sctx, MAX_DEPTH);
	}

	task_set_uclamp(TASK_UCLAMP_MAX);

	if (sctx.interrupted)
		LOGD("sweep: cycle preempted, intr=1");
}

#endif // NDK_BUILD
