// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/sched.h"
#include "pgov/compiler.h"
#include "pgov/log.h"
#include "pgov/node.h"
#include "pgov/parser.h"
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

static const char *const CAND_LAT[] = {
	"/proc/sys/kernel/sched_latency_ns",
	"/sys/kernel/debug/sched/latency_ns",
	NULL,
};

static const char *const CAND_GRAN[] = {
	"/proc/sys/kernel/sched_min_granularity_ns",
	"/sys/kernel/debug/sched/min_granularity_ns",
	NULL,
};

static const char *const CAND_WAKE[] = {
	"/proc/sys/kernel/sched_wakeup_granularity_ns",
	"/sys/kernel/debug/sched/wakeup_granularity_ns",
	NULL,
};

static const char *const CAND_MIG[] = {
	"/proc/sys/kernel/sched_migration_cost_ns",
	"/sys/kernel/debug/sched/migration_cost_ns",
	NULL,
};

static const char *const CAND_WALT[] = {
	"/proc/sys/kernel/sched_walt_init_task_load_pct",
	"/proc/sys/kernel/sched_init_task_load",
	NULL,
};

static const char *const CAND_UCL_MIN[] = {
	"/proc/sys/kernel/sched_uclamp_util_min",
	"/proc/sys/kernel/sched_util_clamp_min",
	NULL,
};

static const char *const CAND_UCL_MAX[] = {
	"/proc/sys/kernel/sched_uclamp_util_max",
	"/proc/sys/kernel/sched_util_clamp_max",
	NULL,
};

#define MAX_NODE 7

struct restore_entry {
	int fd;
	uint64_t base;
};

static struct restore_entry restore_table[MAX_NODE];
static size_t restore_count;

int sched_init(struct context *ctx)
{
	int count = 0;

	count += node_cache_init(&ctx->sched_lat, CAND_LAT);
	count += node_cache_init(&ctx->sched_gran, CAND_GRAN);
	count += node_cache_init(&ctx->sched_wake, CAND_WAKE);
	count += node_cache_init(&ctx->sched_mig, CAND_MIG);
	count += node_cache_init(&ctx->sched_walt, CAND_WALT);
	count += node_cache_init(&ctx->sched_uclmin, CAND_UCL_MIN);
	count += node_cache_init(&ctx->sched_uclmax, CAND_UCL_MAX);

	return count;
}

void sched_snapshot(const struct context *ctx)
{
	const struct node_cache *const nodes[MAX_NODE] = {
		&ctx->sched_lat,    &ctx->sched_gran, &ctx->sched_wake,
		&ctx->sched_mig,    &ctx->sched_walt, &ctx->sched_uclmin,
		&ctx->sched_uclmax,
	};

	size_t i;

	restore_count = 0;

	for (i = 0; i < MAX_NODE; i++) {
		if (!nodes[i]->active || !nodes[i]->has_base)
			continue;

		restore_table[restore_count].fd = nodes[i]->fd;
		restore_table[restore_count].base = nodes[i]->base;
		restore_count++;
	}
}

void sched_restore(void)
{
	char buf[32];
	size_t i;
	size_t len;
	ssize_t res;

	for (i = 0; i < restore_count; i++) {
		len = parse_fmt_u64(restore_table[i].base, buf);
		res = pwrite(restore_table[i].fd, buf, len, 0);
		UNUSED(res);
	}
}

void restore_nodes(struct context *ctx)
{
	node_restore(&ctx->sched_lat);
	node_restore(&ctx->sched_gran);
	node_restore(&ctx->sched_wake);
	node_restore(&ctx->sched_mig);
	node_restore(&ctx->sched_walt);
	node_restore(&ctx->sched_uclmin);
	node_restore(&ctx->sched_uclmax);
	restore_count = 0;
	LOGD("sched: restored tunables");
}

void sched_cleanup(struct context *ctx)
{
	restore_nodes(ctx);
	node_cache_cleanup(&ctx->sched_lat);
	node_cache_cleanup(&ctx->sched_gran);
	node_cache_cleanup(&ctx->sched_wake);
	node_cache_cleanup(&ctx->sched_mig);
	node_cache_cleanup(&ctx->sched_walt);
	node_cache_cleanup(&ctx->sched_uclmin);
	node_cache_cleanup(&ctx->sched_uclmax);
}
