// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#if defined(NDK_BUILD)

#include "opt.h"
#include "compiler.h"
#include "conf.h"
#include "paths.h"
#include "sysfs.h"
#include "prop.h"
#include "str.h"
#include "pg/log.h"
#include <errno.h> // IWYU pragma: keep

#define MAX_PROPS 64

static struct pg_prop_state props[MAX_PROPS];
static size_t prop_count = 0;

static int add_prop(const char *RESTRICT name, const char *RESTRICT val)
{
	if (UNLIKELY(prop_count >= MAX_PROPS)) {
		LOGW("opt: limit %d props, ignoring %s", MAX_PROPS, name);
		return -ENOSPC;
	}

	pg_prop_state_init(&props[prop_count], name);
	int ret = pg_prop_set(&props[prop_count], val);
	if (ret != 0) {
		LOGW("opt: failed to set prop %s err=%d", name, ret);
		return ret;
	}

	prop_count++;
	return 0;
}

static int parse_cb(const char *RESTRICT key, const char *RESTRICT val)
{
	const char *path;
	const char *name;

	if (pg_str_has_prefix(key, "sysfs.")) {
		path = key + 6;
		int ret = pg_sysfs_write(path, val);
		if (ret != 0)
			LOGW("opt: failed write sysfs %s err=%d", path, ret);

		return 0;
	}

	if (pg_str_has_prefix(key, "prop.")) {
		name = key + 5;
		int ret = add_prop(name, val);
		if (ret == -ENOSPC)
			return 0;

		return ret;
	}

	return 0;
}

int pg_opt_init(void)
{
	int ret = pg_conf_parse(CONF_PATH, parse_cb);
	if (ret < 0 && ret != -ENOENT) {
		LOGW("opt: failed to parse conf err=%d", ret);
		return ret;
	}

	if (ret == -ENOENT)
		return 0;

	return ret;
}

void pg_opt_exit(void)
{
	size_t i;

	for (i = 0; i < prop_count; ++i)
		pg_prop_cleanup(&props[i]);

	prop_count = 0;
}

#endif // NDK_BUILD
