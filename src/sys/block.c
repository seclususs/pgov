// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "block.h"
#include "compiler.h"
#include "str.h"
#include "sysfs.h"
#include "pg/log.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define SYS_BLOCK "/sys/block"
#define Q_ROTATIONAL "queue/rotational"
#define Q_ADD_RANDOM "queue/add_random"
#define Q_RQ_AFFINITY "queue/rq_affinity"
#define Q_SCHEDULER "queue/scheduler"
#define Q_FB "queue/iosched/fifo_batch"
#define Q_WS "queue/iosched/writes_starved"
#define Q_FM "queue/iosched/front_merges"
#define Q_SI "queue/iosched/slice_idle"

static const char *const IGNORED[] = { "loop", "ram", "zram", "dm-", "md" };
static const char *const NVME[] = { "kyber", "mq-deadline", "none" };
static const char *const UFS[] = { "mq-deadline", "kyber", "deadline", "none" };
static const char *const EMMC[] = { "mq-deadline", "deadline", "noop", "none" };
static const char *const ROT[] = { "bfq", "mq-deadline", "deadline" };

static inline bool check_ignored(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(IGNORED); ++i) {
		if (pg_str_has_prefix(name, IGNORED[i]))
			return true;
	}

	return false;
}

static inline bool check_rotational(const char *name)
{
	char path[128];
	int32_t val;

	pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_ROTATIONAL);
	if (pg_sysfs_read_i32(path, &val) == 0 && val == 1)
		return true;

	return false;
}

static inline void set_queue(const char *name)
{
	char path[128];

	pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_ADD_RANDOM);
	pg_sysfs_write(path, "0");

	pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_RQ_AFFINITY);
	pg_sysfs_write(path, "1");
}

static int read_sched(const char *RESTRICT name, char *RESTRICT buf, size_t len)
{
	char path[128];
	ssize_t bytes;
	int fd;

	pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_SCHEDULER);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	do {
		bytes = read(fd, buf, len - 1);
	} while (bytes < 0 && errno == EINTR);

	close(fd);

	if (bytes <= 0)
		return (bytes < 0) ? -errno : -ENODATA;

	buf[bytes] = '\0';
	return 0;
}

static const char *select_sched(const char *RESTRICT name,
				const char *RESTRICT avail)
{
	const char *const *prio;
	size_t len;
	size_t i;

	if (check_rotational(name)) {
		prio = ROT;
		len = ARRAY_SIZE(ROT);
	} else if (pg_str_has_prefix(name, "nvme")) {
		prio = NVME;
		len = ARRAY_SIZE(NVME);
	} else if (pg_str_has_prefix(name, "mmcblk")) {
		prio = EMMC;
		len = ARRAY_SIZE(EMMC);
	} else {
		prio = UFS;
		len = ARRAY_SIZE(UFS);
	}

	for (i = 0; i < len; ++i) {
		if (pg_str_contains(avail, prio[i]))
			return prio[i];
	}

	return NULL;
}

static inline void set_sched(const char *RESTRICT name,
			     const char *RESTRICT sched)
{
	char path[128];

	if (strcmp(sched, "mq-deadline") == 0 ||
	    strcmp(sched, "deadline") == 0) {
		pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_FB);
		pg_sysfs_write(path, "16");

		pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_WS);
		pg_sysfs_write(path, "2");

		pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_FM);
		pg_sysfs_write(path, "1");

		return;
	}

	if (strcmp(sched, "bfq") == 0) {
		pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_SI);
		pg_sysfs_write(path, "0");
	}
}

static int apply_block_tweaks(const char *name)
{
	char buf[128];
	char path[128];
	const char *sched;
	int ret;

	set_queue(name);

	ret = read_sched(name, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	sched = select_sched(name, buf);
	if (!sched)
		return -ENOENT;

	LOGD("block: set scheduler %s for %s", sched, name);
	pg_str_build_path(path, sizeof(path), SYS_BLOCK, name, Q_SCHEDULER);

	ret = pg_sysfs_write(path, sched);
	if (ret < 0)
		return ret;

	set_sched(name, sched);

	return 0;
}

int pg_block_tune(void)
{
	struct dirent *ent;
	DIR *dir;

	dir = opendir(SYS_BLOCK);
	if (!dir)
		return -errno;

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		if (check_ignored(ent->d_name))
			continue;

		apply_block_tweaks(ent->d_name);
	}

	closedir(dir);
	return 0;
}

#endif // NDK_BUILD
