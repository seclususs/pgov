// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "sensor.h"
#include "scan.h"
#include "sysfs.h"
#include "pg/log.h"
#include "parser.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#define SCALE_MILLI 1000
#define SCALE_DECI 10
#define FALLBACK_TEMP_CPU INT_TO_Q16(65)
#define FALLBACK_TEMP_BAT INT_TO_Q16(35)
#define FALLBACK_BAT_CAP INT_TO_Q16(100)
#define FALLBACK_BL 0

void pg_sensor_init_cpu_temp(struct pg_temp_sensor *RESTRICT sensor,
			     const char *RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	sensor->scale = SCALE_MILLI;
	if (sensor->fd < 0)
		LOGW("sensor: failed to open node %s err=%d", path, errno);
}

void pg_sensor_init_bat_temp(struct pg_temp_sensor *RESTRICT sensor,
			     const char *RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	sensor->scale = SCALE_DECI;
	if (sensor->fd < 0)
		LOGW("sensor: failed to open node %s err=%d", path, errno);
}

void pg_sensor_temp_close(struct pg_temp_sensor *sensor)
{
	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

void pg_sensor_init_bat_cap(struct pg_bat_sensor *RESTRICT sensor,
			    const char *RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (sensor->fd < 0)
		LOGW("sensor: failed to open node %s err=%d", path, errno);
}

void pg_sensor_bat_close(struct pg_bat_sensor *sensor)
{
	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

void pg_sensor_init_bl(struct pg_bl_sensor *RESTRICT sensor,
		       const char *RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (sensor->fd < 0)
		LOGW("sensor: failed to open node %s err=%d", path, errno);
}

void pg_sensor_bl_close(struct pg_bl_sensor *sensor)
{
	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

static inline int read_temp(struct pg_temp_sensor *RESTRICT sensor,
			    q16_t *RESTRICT temp, q16_t fallback)
{
	ssize_t sz;

	if (sensor->fd < 0) {
		*temp = fallback;
		return -EBADF;
	}

	do {
		sz = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	} while (sz < 0 && errno == EINTR);

	if (sz <= 0) {
		*temp = fallback;
		return -EIO;
	}

	bool valid;
	int32_t val = pg_parse_i32(sensor->buf, (size_t)sz, &valid);
	if (!valid) {
		*temp = fallback;
		return -EINVAL;
	}

	*temp = (q16_t)(((q32_t)val * Q16_ONE) / sensor->scale);
	return 0;
}

int pg_sensor_read_cpu_temp(struct pg_temp_sensor *RESTRICT sensor,
			    q16_t *RESTRICT temp)
{
	return read_temp(sensor, temp, FALLBACK_TEMP_CPU);
}

int pg_sensor_read_bat_temp(struct pg_temp_sensor *RESTRICT sensor,
			    q16_t *RESTRICT temp)
{
	return read_temp(sensor, temp, FALLBACK_TEMP_BAT);
}

int pg_sensor_read_bat_cap(struct pg_bat_sensor *RESTRICT sensor,
			   q16_t *RESTRICT cap)
{
	ssize_t sz;

	if (sensor->fd < 0) {
		*cap = FALLBACK_BAT_CAP;
		return -EBADF;
	}

	do {
		sz = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	} while (sz < 0 && errno == EINTR);

	if (sz <= 0) {
		*cap = FALLBACK_BAT_CAP;
		return -EIO;
	}

	bool valid;
	int32_t val = pg_parse_i32(sensor->buf, (size_t)sz, &valid);
	if (UNLIKELY(!valid)) {
		*cap = FALLBACK_BAT_CAP;
		return -EINVAL;
	}

	*cap = INT_TO_Q16(val);
	return 0;
}

int pg_sensor_read_bl(struct pg_bl_sensor *RESTRICT sensor,
		      int32_t *RESTRICT brightness)
{
	ssize_t sz;

	if (sensor->fd < 0) {
		*brightness = FALLBACK_BL;
		return -EBADF;
	}

	do {
		sz = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	} while (sz < 0 && errno == EINTR);

	if (sz <= 0) {
		*brightness = FALLBACK_BL;
		return -EIO;
	}

	bool valid;
	int32_t val = pg_parse_i32(sensor->buf, (size_t)sz, &valid);
	if (UNLIKELY(!valid)) {
		*brightness = FALLBACK_BL;
		return -EINVAL;
	}

	*brightness = val;
	return 0;
}

q16_t pg_sensor_get_trip_temp(q16_t fallback)
{
	char path[256];
	int32_t val;
	q16_t temp;

	if (pg_scan_trip_point(path, sizeof(path)) != 0)
		return fallback;

	if (pg_sysfs_read_i32(path, &val) != 0)
		return fallback;

	temp = (q16_t)(((q32_t)val * Q16_ONE) / 1000);
	if (temp < INT_TO_Q16(40) || temp > INT_TO_Q16(115))
		return fallback;

	return temp;
}
