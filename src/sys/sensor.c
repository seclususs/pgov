// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "sensor.h"
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

static inline int read_temp(struct pg_temp_sensor *RESTRICT sensor,
			    q16_t *RESTRICT temp, q16_t fallback)
{
	if (sensor->fd < 0) {
		*temp = fallback;
		return -EBADF;
	}

	ssize_t sz = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
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

	*temp = (q16_t)(((q32_t)val << 16) / sensor->scale);
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
	if (sensor->fd < 0) {
		*cap = FALLBACK_BAT_CAP;
		return -EBADF;
	}

	ssize_t sz = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	if (sz <= 0) {
		*cap = FALLBACK_BAT_CAP;
		return -EIO;
	}

	bool valid;
	int32_t val = pg_parse_i32(sensor->buf, (size_t)sz, &valid);
	*cap = (int)valid ? INT_TO_Q16(val) : FALLBACK_BAT_CAP;
	return 0;
}
