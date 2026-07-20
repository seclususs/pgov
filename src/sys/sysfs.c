// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "sysfs.h"
#include "parser.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void pg_sysfs_cache_init(struct pg_sysfs_cache *RESTRICT cache,
			 const char *RESTRICT path)
{
	cache->fd = open(path, O_WRONLY | O_CLOEXEC);
	if (cache->fd >= 0) {
		cache->active = true;
		cache->last = UINT64_MAX;
	} else {
		cache->active = false;
	}
}

void pg_sysfs_cache_cleanup(struct pg_sysfs_cache *cache)
{
	if (cache->fd >= 0) {
		close(cache->fd);
		cache->fd = -1;
	}

	cache->active = false;
}

int pg_sysfs_read_i32(const char *path, int32_t *out_val)
{
	int ret = 0;

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	char buf[32];
	ssize_t bytes;
	do {
		bytes = read(fd, buf, sizeof(buf));
	} while (bytes < 0 && errno == EINTR);

	if (bytes <= 0) {
		ret = (bytes < 0) ? -errno : -ENODATA;
		goto out;
	}

	bool valid;
	int32_t val = pg_parse_i32((const uint8_t *)buf, (size_t)bytes, &valid);
	if (!valid) {
		ret = -EINVAL;
		goto out;
	}

	*out_val = val;

out:
	close(fd);
	return ret;
}

int pg_sysfs_write_strm(int fd, uint64_t value)
{
	ssize_t res;

	if (fd < 0)
		return -EBADF;

	char buf[32];
	size_t len = pg_fmt_u64(value, buf);

	do {
		res = pwrite(fd, buf, len, 0);
	} while (res < 0 && errno == EINTR);

	if (res < 0)
		return -errno;

	if ((size_t)res != len)
		return -EIO;

	return 0;
}

int pg_sysfs_write(const char *RESTRICT path, const char *RESTRICT val)
{
	ssize_t res;
	int ret = 0;

	if (UNLIKELY(!path || !val))
		return -EINVAL;

	int fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	size_t len = 0;
	while (val[len] != '\0')
		len++;

	do {
		res = pwrite(fd, val, len, 0);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		ret = -errno;
		goto out;
	}

	if ((size_t)res != len) {
		ret = -EIO;
		goto out;
	}

out:
	close(fd);
	return ret;
}

static inline bool absolute(uint64_t cur, uint64_t tgt, uint64_t thresh)
{
	if (UNLIKELY(cur == UINT64_MAX))
		return true;

	if (cur == tgt)
		return false;

	uint64_t diff = (cur > tgt) ? (cur - tgt) : (tgt - cur);
	return diff >= thresh;
}

static inline bool relative(uint64_t cur, uint64_t tgt, uint64_t tol)
{
	if (UNLIKELY(cur == UINT64_MAX))
		return true;

	if (cur == tgt)
		return false;

	if (cur == 0)
		return tgt != 0;

	uint64_t diff = (cur > tgt) ? (cur - tgt) : (tgt - cur);
	return (diff * (uint64_t)1000) >= (cur * tol);
}

void pg_sysfs_update(struct pg_sysfs_cache *RESTRICT cache, uint64_t value,
		     bool force, const struct pg_sysfs_chk *RESTRICT strat)
{
	if (!cache->active)
		return;

	bool update = false;

	if (force)
		update = true;
	else {
		switch (strat->type) {
		case PG_CHK_ABS:
			update = absolute(cache->last, value, strat->thresh);
			break;

		case PG_CHK_REL:
			update = relative(cache->last, value, strat->thresh);
			break;

		case PG_CHK_STRICT:
			update = (cache->last != value);
			break;
		}
	}

	if (update && pg_sysfs_write_strm(cache->fd, value) == 0)
		cache->last = value;
}
