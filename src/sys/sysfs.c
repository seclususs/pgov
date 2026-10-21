// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "sysfs.h"
#include "parser.h"
#include "pg/log.h"
#include "pg/config.h"
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define SYSFS_BACKOFF_BASE_MS 1000U
#define SYSFS_BACKOFF_MAX_MS 60000U

static inline uint32_t mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(((uint64_t)ts.tv_sec * 1000U) +
			  ((uint64_t)ts.tv_nsec / 1000000U));
}

int pg_sysfs_cache_init(struct pg_sysfs_cache *RESTRICT cache,
			const char *const *RESTRICT paths)
{
	if (UNLIKELY(!cache || !paths))
		return -EINVAL;

	cache->active = false;
	cache->has_base = false;
	cache->fails = 0;
	cache->retry_ms = 0;
	cache->last = UINT64_MAX;
	cache->base = 0;
	cache->fd = -1;

	for (size_t i = 0; paths[i] != NULL; i++) {
		int fd = open(paths[i], O_WRONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		int32_t cur = -1;
		cache->fd = fd;
		cache->active = true;

		if (pg_sysfs_read_i32(paths[i], &cur) == 0 && cur >= 0) {
			cache->base = (uint32_t)cur;
			cache->has_base = true;
		}

		LOGI("sysfs: node mapped to %s", paths[i]);
		return 1;
	}

	LOGW("sysfs: no usable node (candidate %s)", paths[0]);
	return 0;
}

void pg_sysfs_restore(struct pg_sysfs_cache *cache)
{
	if (UNLIKELY(!cache))
		return;

	if (!cache->active || !cache->has_base || cache->last == UINT64_MAX ||
	    cache->last == cache->base)
		return;

	if (pg_sysfs_write_strm(cache->fd, cache->base) == 0)
		cache->last = cache->base;
}

void pg_sysfs_cache_cleanup(struct pg_sysfs_cache *cache)
{
	if (UNLIKELY(!cache))
		return;

	if (cache->fd >= 0) {
		close(cache->fd);
		cache->fd = -1;
	}

	cache->active = false;
}

int pg_sysfs_read_i32(const char *path, int32_t *out_val)
{
	int ret = 0;

	if (UNLIKELY(!path || !out_val))
		return -EINVAL;

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		int err = errno;
		LOGE("sysfs: failed to open read %s err=%d", path, err);
		return -err;
	}

	char buf[32];
	ssize_t bytes;
	do {
		bytes = read(fd, buf, sizeof(buf));
	} while (bytes < 0 && errno == EINTR);

	if (bytes <= 0) {
		ret = (bytes < 0) ? -errno : -ENODATA;
		LOGE("sysfs: failed to read data %s err=%d", path, -ret);
		goto out;
	}

	bool valid;
	int32_t val = pg_parse_i32((const uint8_t *)buf, (size_t)bytes, &valid);
	if (!valid) {
		ret = -EINVAL;
		LOGE("sysfs: failed to parse valid i32 from %s", path);
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

	if (res < 0) {
		int err = errno;
		LOGE("sysfs: failed to write strm fd=%d err=%d", fd, err);
		return -err;
	}

	if ((size_t)res != len) {
		LOGE("sysfs: incomplete strm write fd=%d", fd);
		return -EIO;
	}

	return 0;
}

int pg_sysfs_write(const char *RESTRICT path, const char *RESTRICT val)
{
	ssize_t res;
	int ret = 0;

	if (UNLIKELY(!path || !val))
		return -EINVAL;

	int fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		int err = errno;
		LOGE("sysfs: failed to open write %s err=%d", path, err);
		return -err;
	}

	size_t len = 0;
	while (val[len] != '\0')
		len++;

	char buf[64];
	const char *w_val = val;

	if (len > 0 && val[len - 1] != '\n' && len < sizeof(buf) - 1) {
		for (size_t i = 0; i < len; i++)
			buf[i] = val[i];

		buf[len] = '\n';
		len++;
		w_val = buf;
	}

	do {
		res = pwrite(fd, w_val, len, 0);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		ret = -errno;
		LOGE("sysfs: failed to write %s err=%d", path, -ret);
		goto out;
	}

	if ((size_t)res != len) {
		ret = -EIO;
		LOGE("sysfs: incomplete write %s", path);
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
	if (UNLIKELY(!cache))
		return;

	if (!cache->active)
		return;

	bool update = false;

	if (force) {
		update = true;
	} else {
		if (UNLIKELY(!strat))
			return;

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

	if (!update)
		return;

	if (UNLIKELY(cache->fails != 0) &&
	    (int32_t)(mono_ms() - cache->retry_ms) < 0)
		return;

	if (pg_sysfs_write_strm(cache->fd, value) == 0) {
		cache->last = value;
		cache->fails = 0;
		return;
	}

	if (cache->fails < PG_SYSFS_FAIL_MAX)
		cache->fails++;

	uint32_t delay = SYSFS_BACKOFF_BASE_MS << (cache->fails - 1);
	if (delay > SYSFS_BACKOFF_MAX_MS)
		delay = SYSFS_BACKOFF_MAX_MS;

	cache->retry_ms = mono_ms() + delay;
}
