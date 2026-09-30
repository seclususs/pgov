// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/log.h"
#include "compiler.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_BURST 128U
#define LOG_REFILL_MS 10000ULL
#define LOG_CREDIT_MAX ((uint64_t)LOG_BURST * LOG_REFILL_MS)

void pg_log_err(const char *tag, const char *fmt, ...)
{
	static uint64_t credit_ms = LOG_CREDIT_MAX;
	static uint64_t last_ms;
	static uint32_t dropped;

	struct timespec mono;

	clock_gettime(CLOCK_MONOTONIC, &mono);

	uint64_t now_ms = ((uint64_t)mono.tv_sec * 1000U) +
			  ((uint64_t)mono.tv_nsec / 1000000U);

	if (last_ms != 0) {
		credit_ms += now_ms - last_ms;
		if (credit_ms > LOG_CREDIT_MAX)
			credit_ms = LOG_CREDIT_MAX;
	}
	last_ms = now_ms;

	if (credit_ms < LOG_REFILL_MS) {
		dropped++;
		return;
	}
	credit_ms -= LOG_REFILL_MS;

	char msg[256];
	va_list args;

	va_start(args, fmt);
	(void)vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (dropped != 0) {
		size_t len = strlen(msg);

		(void)snprintf(msg + len, sizeof(msg) - len,
			       " (+%u suppressed)", dropped);
		dropped = 0;
	}

	__android_log_write(ANDROID_LOG_ERROR, tag, msg);

	time_t now = time(NULL);
	struct tm tm;
	char ts[24] = { 0 };

	if (localtime_r(&now, &tm))
		(void)strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

	char out[300];
	int n = snprintf(out, sizeof(out), "[%s] [%s] [E] %s\n", ts, tag, msg);
	if (n > 0) {
		size_t max = sizeof(out) - 1;
		size_t w = (size_t)n < max ? (size_t)n : max;
		ssize_t unused = write(STDERR_FILENO, out, w);
		UNUSED(unused);
	}
}
