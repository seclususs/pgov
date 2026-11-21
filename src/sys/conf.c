// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "pgov/conf.h"
#include "pgov/compiler.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define LINE_SIZE 256

static inline void trim_trailing(char *str)
{
	char *tmp;

	tmp = str;
	while (*tmp != '\0')
		tmp++;

	if (tmp > str) {
		tmp--;

		while (tmp >= str && (*tmp == ' ' || *tmp == '\t')) {
			*tmp = '\0';
			tmp--;
		}
	}
}

static int parse_line(char *line, conf_callback cb)
{
	char *key;
	char *tmp;
	char *val;

	key = line;
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

	trim_trailing(key);

	while (*val == ' ' || *val == '\t')
		val++;

	trim_trailing(val);

	if (UNLIKELY(*key == '\0'))
		return 0;

	return cb(key, val);
}

static inline ssize_t refill_buffer(int fd, char *buf, size_t size)
{
	ssize_t res;

	do {
		res = read(fd, buf, size);
	} while (res < 0 && errno == EINTR);

	return res;
}

static inline int dispatch_line(char *line, size_t len, conf_callback cb)
{
	if (len == 0)
		return 0;

	line[len] = '\0';
	return parse_line(line, cb);
}

int conf_parse(const char *path, conf_callback cb)
{
	char buf[BUF_SIZE];
	char line[LINE_SIZE];
	char c;
	size_t i = 0;
	size_t len = 0;
	ssize_t res = 0;
	int fd;
	int ret = 0;
	bool skip = false;

	if (UNLIKELY(!path || !cb))
		return -EINVAL;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	for (;;) {
		if (i >= (size_t)res) {
			res = refill_buffer(fd, buf, sizeof(buf));
			if (res <= 0)
				break;
			i = 0;
		}

		c = buf[i++];

		if (c == '\n') {
			if (UNLIKELY(skip)) {
				skip = false;
				len = 0;
				continue;
			}

			ret = dispatch_line(line, len, cb);
			len = 0;
			if (ret < 0)
				goto out;

			continue;
		}

		if (UNLIKELY(skip))
			continue;

		if (UNLIKELY(len >= sizeof(line) - 1)) {
			skip = true;
			continue;
		}

		line[len++] = c;
	}

	if (res < 0) {
		ret = -errno;
		goto out;
	}

	if (!skip)
		ret = dispatch_line(line, len, cb);

out:
	close(fd);
	return ret;
}

#endif // NDK_BUILD
