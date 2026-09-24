// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pg/log.h"
#include "compiler.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

void pg_log_err(const char *tag, const char *fmt, ...)
{
	char msg[256];
	va_list args;

	va_start(args, fmt);
	(void)vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

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
