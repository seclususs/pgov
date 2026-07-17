// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "conf.h"
#include <errno.h>
#include <fcntl.h>
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

	if (UNLIKELY(*key == '\0'))
		return 0;

	return cb(key, val);
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

	if (UNLIKELY(!path || !cb))
		return -EINVAL;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	for (;;) {
		if (i >= (size_t)sz) {
			sz = read(fd, buf, sizeof(buf));

			if (sz <= 0)
				break;

			i = 0;
		}

		char c = buf[i++];

		if (c == '\n' || l_pos >= sizeof(line) - 1) {
			line[l_pos] = '\0';

			ret = parse_line(line, cb);
			if (ret < 0)
				goto out;

			l_pos = 0;
			continue;
		}

		line[l_pos++] = c;
	}

	if (sz < 0) {
		ret = -errno;
		goto out;
	}

	if (l_pos > 0) {
		line[l_pos] = '\0';
		ret = parse_line(line, cb);
	}

out:
	close(fd);
	return ret;
}

#endif // NDK_BUILD
