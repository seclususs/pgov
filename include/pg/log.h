// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_LOG_H
#define PG_LOG_H

#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "PGOV"
#endif

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#if defined(NDEBUG) && !defined(ENABLE_VERBOSE_LOGS)
#define LOGD(...) \
	do {      \
	} while (0)
#else
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#endif

#endif // PG_LOG_H
