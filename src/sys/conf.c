// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "conf.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define CONF_BUF_SZ 4096
#define CONF_LINE_SZ 256

static int parse_line(char *RESTRICT line, pg_conf_cb cb)
{
	char *key = line;
	char *val;
	char *tmp;

	tmp = strchr(key, '#');
	if (tmp)
		*tmp = '\0';

	tmp = strchr(key, '\r');
	if (tmp)
		*tmp = '\0';

	val = strchr(key, '=');
	if (!val)
		return 0;

	*val = '\0';
	val++;

	while (*key == ' ' || *key == '\t')
		key++;

	tmp = key;
	while (*tmp != '\0')
		tmp++;

	if (tmp > key) {
		tmp--;

		while (tmp >= key && (*tmp == ' ' || *tmp == '\t')) {
			*tmp = '\0';
			tmp--;
		}
	}

	while (*val == ' ' || *val == '\t')
		val++;

	tmp = val;
	while (*tmp != '\0')
		tmp++;

	if (tmp > val) {
		tmp--;

		while (tmp >= val && (*tmp == ' ' || *tmp == '\t')) {
			*tmp = '\0';
			tmp--;
		}
	}

	if (UNLIKELY(*key == '\0'))
		return 0;

	return cb(key, val);
}

static inline ssize_t refill_buffer(int fd, char *buf, size_t size)
{
	ssize_t sz;

	do {
		sz = read(fd, buf, size);
	} while (sz < 0 && errno == EINTR);

	return sz;
}

static inline int dispatch_line(char *line, size_t len, pg_conf_cb cb)
{
	if (len == 0)
		return 0;

	line[len] = '\0';
	return parse_line(line, cb);
}

int pg_conf_parse(const char *RESTRICT path, pg_conf_cb cb)
{
	char buf[CONF_BUF_SZ];
	char line[CONF_LINE_SZ];
	size_t l_pos = 0;
	size_t i = 0;
	ssize_t sz = 0;
	int ret = 0;
	int fd;
	bool skip = false;

	if (UNLIKELY(!path || !cb))
		return -EINVAL;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	for (;;) {
		if (i >= (size_t)sz) {
			sz = refill_buffer(fd, buf, sizeof(buf));
			if (sz <= 0)
				break;
			i = 0;
		}

		char c = buf[i++];

		if (c == '\n') {
			if (UNLIKELY(skip)) {
				skip = false;
				l_pos = 0;
				continue;
			}

			ret = dispatch_line(line, l_pos, cb);
			l_pos = 0;
			if (ret < 0)
				goto out;

			continue;
		}

		if (UNLIKELY(skip))
			continue;

		if (UNLIKELY(l_pos >= sizeof(line) - 1)) {
			skip = true;
			continue;
		}

		line[l_pos++] = c;
	}

	if (sz < 0) {
		ret = -errno;
		goto out;
	}

	if (!skip)
		ret = dispatch_line(line, l_pos, cb);

out:
	close(fd);
	return ret;
}

#endif // NDK_BUILD
