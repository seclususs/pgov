// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/log.h"
#include "pgov/compiler.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BURST_LIMIT 128U
#define REFILL_MS 10000ULL
#define CREDIT_MAX ((uint64_t)BURST_LIMIT * REFILL_MS)
#define STDERR_FMT "[%s] [%s] [%s] %s\n"

static bool check_rate_limit(uint32_t *out_dropped)
{
	static uint64_t credit_ms = CREDIT_MAX;
	static uint64_t last_ms;
	static uint32_t dropped;
	struct timespec mono;
	uint64_t now_ms;

	clock_gettime(CLOCK_MONOTONIC, &mono);

	now_ms = ((uint64_t)mono.tv_sec * 1000U) +
		 ((uint64_t)mono.tv_nsec / 1000000U);

	if (last_ms != 0) {
		credit_ms += now_ms - last_ms;
		if (credit_ms > CREDIT_MAX)
			credit_ms = CREDIT_MAX;
	}
	last_ms = now_ms;

	if (credit_ms < REFILL_MS) {
		dropped++;
		return true;
	}
	credit_ms -= REFILL_MS;

	*out_dropped = dropped;
	dropped = 0;
	return false;
}

static void write_stderr(const char *tag, const char *level, const char *msg)
{
	char out[300];
	char ts[24] = { 0 };
	struct tm tm;
	time_t now;
	size_t len;
	size_t max;
	ssize_t res;
	int ret;

	now = time(NULL);
	if (localtime_r(&now, &tm))
		(void)strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

	ret = snprintf(out, sizeof(out), STDERR_FMT, ts, tag, level, msg);
	if (ret > 0) {
		max = sizeof(out) - 1;
		len = (size_t)ret < max ? (size_t)ret : max;
		res = write(STDERR_FILENO, out, len);
		UNUSED(res);
	}
}

void log_err(const char *tag, const char *fmt, ...)
{
	char msg[256];
	va_list args;
	size_t len = 0;
	uint32_t dropped = 0;
	int ret;

	if (UNLIKELY(!tag || !fmt))
		return;

	if (check_rate_limit(&dropped))
		return;

	va_start(args, fmt);
	ret = vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (dropped != 0) {
		if (ret > 0) {
			len = (size_t)ret;
			if (len >= sizeof(msg))
				len = sizeof(msg) - 1;
		}

		(void)snprintf(msg + len, sizeof(msg) - len,
			       " (+%u suppressed)", dropped);
	}

	__android_log_write(ANDROID_LOG_ERROR, tag, msg);

	write_stderr(tag, "E", msg);
}

void log_msg(int prio, const char *tag, const char *fmt, ...)
{
	const char *level;
	char msg[256];
	va_list args;
	int ret;

	if (UNLIKELY(!tag || !fmt))
		return;

	va_start(args, fmt);
	ret = vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (ret < 0)
		return;

	__android_log_write(prio, tag, msg);

	level = (prio == ANDROID_LOG_WARN) ? "W" : "I";
	write_stderr(tag, level, msg);
}
