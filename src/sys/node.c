// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/node.h"
#include "pgov/parser.h"
#include "pgov/log.h"
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define FAIL_MAX 7
#define BACKOFF_BASE_MS 1000U
#define BACKOFF_MAX_MS 60000U

static inline uint32_t get_monotonic_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(((uint64_t)ts.tv_sec * 1000U) +
			  ((uint64_t)ts.tv_nsec / 1000000U));
}

int node_cache_init(struct node_cache *RESTRICT cache,
		    const char *const *RESTRICT paths)
{
	int32_t cur;
	size_t i;
	int fd;

	if (UNLIKELY(!cache || !paths))
		return -EINVAL;

	cache->active = false;
	cache->has_base = false;
	cache->fails = 0;
	cache->retry_ms = 0;
	cache->last = UINT64_MAX;
	cache->base = 0;
	cache->fd = -1;

	for (i = 0; paths[i] != NULL; i++) {
		fd = open(paths[i], O_WRONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		cur = -1;
		cache->fd = fd;
		cache->active = true;

		if (node_read_i32(paths[i], &cur) == 0 && cur >= 0) {
			cache->base = (uint32_t)cur;
			cache->has_base = true;
		}

		LOGI("node: mapped to %s", paths[i]);
		return 1;
	}

	LOGW("node: no usable (candidate %s)", paths[0]);
	return 0;
}

void node_restore(struct node_cache *cache)
{
	if (UNLIKELY(!cache))
		return;

	if (!cache->active || !cache->has_base || cache->last == UINT64_MAX ||
	    cache->last == cache->base)
		return;

	if (node_write_stream(cache->fd, cache->base) == 0)
		cache->last = cache->base;
}

void node_cache_cleanup(struct node_cache *cache)
{
	if (UNLIKELY(!cache))
		return;

	if (cache->fd >= 0) {
		close(cache->fd);
		cache->fd = -1;
	}

	cache->active = false;
}

int node_read_i32(const char *path, int32_t *out_val)
{
	char buf[32];
	ssize_t res;
	int32_t val;
	bool valid;
	int fd;
	int err;

	if (UNLIKELY(!path || !out_val))
		return -EINVAL;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		err = errno;
		LOGE("node: failed to open read %s err=%d", path, err);
		return -err;
	}

	do {
		res = read(fd, buf, sizeof(buf));
	} while (res < 0 && errno == EINTR);

	close(fd);

	if (res <= 0) {
		err = (res < 0) ? errno : ENODATA;
		LOGE("node: failed to read data %s err=%d", path, err);
		return -err;
	}

	val = parse_i32((const uint8_t *)buf, (size_t)res, &valid);
	if (!valid) {
		LOGE("node: failed to parse valid i32 from %s", path);
		return -EINVAL;
	}

	*out_val = val;
	return 0;
}

ssize_t node_read_str(const char *path, char *buf, size_t len)
{
	ssize_t res;
	int fd;

	if (UNLIKELY(!path || !buf || len == 0))
		return -1;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	do {
		res = read(fd, buf, len - 1);
	} while (res < 0 && errno == EINTR);

	close(fd);

	if (res <= 0)
		return -1;

	buf[res] = '\0';
	for (ssize_t i = 0; i < res; ++i) {
		if (buf[i] == '\n' || buf[i] == '\r') {
			buf[i] = '\0';
			res = i;
			break;
		}
	}

	return res;
}

int node_write_stream(int fd, uint64_t val)
{
	char buf[32];
	size_t len;
	ssize_t res;
	int err;

	if (fd < 0)
		return -EBADF;

	len = parse_fmt_u64(val, buf);

	do {
		res = pwrite(fd, buf, len, 0);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
		err = errno;
		LOGE("node: failed to write stream fd=%d err=%d", fd, err);
		return -err;
	}

	if ((size_t)res != len) {
		LOGE("node: incomplete stream write fd=%d", fd);
		return -EIO;
	}

	return 0;
}

int node_write(const char *path, const char *val)
{
	char buf[64];
	const char *data;
	size_t len = 0;
	size_t i;
	ssize_t res;
	int fd;
	int err;

	if (UNLIKELY(!path || !val))
		return -EINVAL;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		err = errno;
		LOGE("node: failed to open write %s err=%d", path, err);
		return -err;
	}

	while (val[len] != '\0')
		len++;

	data = val;
	if (len > 0 && val[len - 1] != '\n' && len < sizeof(buf) - 1) {
		for (i = 0; i < len; i++)
			buf[i] = val[i];

		buf[len] = '\n';
		len++;
		data = buf;
	}

	do {
		res = pwrite(fd, data, len, 0);
	} while (res < 0 && errno == EINTR);

	close(fd);

	if (res < 0) {
		err = errno;
		LOGE("node: failed to write %s err=%d", path, err);
		return -err;
	}

	if ((size_t)res != len) {
		LOGE("node: incomplete write %s", path);
		return -EIO;
	}

	return 0;
}

static inline bool check_absolute(uint64_t cur, uint64_t target,
				  uint64_t thresh)
{
	uint64_t diff;

	if (UNLIKELY(cur == UINT64_MAX))
		return true;

	if (cur == target)
		return false;

	diff = (cur > target) ? (cur - target) : (target - cur);
	return diff >= thresh;
}

static inline bool check_relative(uint64_t cur, uint64_t target,
				  uint64_t thresh)
{
	uint64_t diff;

	if (UNLIKELY(cur == UINT64_MAX))
		return true;

	if (cur == target)
		return false;

	if (cur == 0)
		return target != 0;

	diff = (cur > target) ? (cur - target) : (target - cur);
	return (diff * (uint64_t)1000) >= (cur * thresh);
}

void node_update(struct node_cache *cache, uint64_t val, bool force,
		 const struct node_check *check)
{
	uint32_t now_ms;
	uint32_t delay;
	bool update = false;

	if (UNLIKELY(!cache))
		return;

	if (!cache->active)
		return;

	if (force) {
		update = true;
	} else {
		if (UNLIKELY(!check))
			return;

		switch (check->type) {
		case NODE_CHECK_ABSOLUTE:
			update =
				check_absolute(cache->last, val, check->thresh);
			break;

		case NODE_CHECK_RELATIVE:
			update =
				check_relative(cache->last, val, check->thresh);
			break;

		case NODE_CHECK_STRICT:
			update = (cache->last != val);
			break;
		}
	}

	if (!update)
		return;

	now_ms = get_monotonic_ms();
	if (UNLIKELY(cache->fails != 0) &&
	    (int32_t)(now_ms - cache->retry_ms) < 0)
		return;

	if (node_write_stream(cache->fd, val) == 0) {
		cache->last = val;
		cache->fails = 0;
		return;
	}

	if (cache->fails < FAIL_MAX)
		cache->fails++;

	delay = BACKOFF_BASE_MS << (cache->fails - 1);
	if (delay > BACKOFF_MAX_MS)
		delay = BACKOFF_MAX_MS;

	cache->retry_ms = now_ms + delay;
}
