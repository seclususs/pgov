// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "pgov/opt.h"
#include "pgov/compiler.h"
#include "pgov/conf.h"
#include "pgov/node.h"
#include "pgov/prop.h"
#include "pgov/str.h"
#include "pgov/log.h"
#include <errno.h> // IWYU pragma: keep

#define PATH_CONF "/data/adb/modules/pgovd/system/etc/pgovd.conf"
#define MAX_PROPS 64

static struct prop_state state[MAX_PROPS];
static size_t prop_count = 0;

static int add_prop(const char *name, const char *val)
{
	int ret;

	if (UNLIKELY(prop_count >= MAX_PROPS)) {
		LOGW("opt: limit %d props, ignoring %s", MAX_PROPS, name);
		return -ENOSPC;
	}

	prop_init(&state[prop_count], name);
	ret = prop_set(&state[prop_count], val);
	if (ret != 0) {
		LOGW("opt: failed to set prop %s err=%d", name, ret);
		return ret;
	}

	prop_count++;
	return 0;
}

static int parse_cb(const char *key, const char *val)
{
	const char *name;
	const char *path;
	int ret;

	if (str_has_prefix(key, "node.")) {
		path = key + 5;
		ret = node_write(path, val);
		if (ret != 0)
			LOGW("opt: failed write node %s err=%d", path, ret);

		return 0;
	}

	if (str_has_prefix(key, "prop.")) {
		name = key + 5;
		add_prop(name, val);
		return 0;
	}

	return 0;
}

int opt_init(void)
{
	int ret;

	ret = conf_parse(PATH_CONF, parse_cb);
	if (ret < 0 && ret != -ENOENT) {
		LOGW("opt: failed to parse conf err=%d", ret);
		return ret;
	}

	if (ret == -ENOENT)
		return 0;

	return ret;
}

void opt_exit(void)
{
	size_t i;

	for (i = prop_count; i > 0; --i)
		prop_cleanup(&state[i - 1]);

	prop_count = 0;
}

#endif // NDK_BUILD
