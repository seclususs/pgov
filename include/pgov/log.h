// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_LOG_H
#define PGOV_LOG_H

#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "PGOV"
#endif

void log_err(const char *tag, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

void log_msg(int prio, const char *tag, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

#define LOGE(...) log_err(LOG_TAG, __VA_ARGS__)
#define LOGW(...) log_msg(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGI(...) log_msg(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#if defined(NDEBUG) && !defined(ENABLE_VERBOSE_LOGS)
#define LOGD(...) \
	do {      \
	} while (0)
#else
#define LOGD(...)                                                             \
	do {                                                                  \
		__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__); \
	} while (0)
#endif

#endif // PGOV_LOG_H
