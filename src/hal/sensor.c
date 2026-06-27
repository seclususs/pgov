// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/sensor.h"
#include "daemon/logger.h"
#include "pascal_gov/parser.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#define SCALE_MILLI 0.001F
#define SCALE_DECI 0.1F
#define FALLBACK_TEMP_CPU 65.0F
#define FALLBACK_TEMP_BAT 35.0F
#define FALLBACK_BAT_CAPACITY 100.0F

void pascal_gov_sensor_init_cpu_thermal(
	pascal_gov_thermal_sensor *PASCAL_GOV_RESTRICT sensor,
	const char *PASCAL_GOV_RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	sensor->scale_multiplier = SCALE_MILLI;
	if (sensor->fd < 0) {
		LOGW("sensor: failed to open node %s err=%d", path, errno);
	}
}

void pascal_gov_sensor_init_bat_thermal(
	pascal_gov_thermal_sensor *PASCAL_GOV_RESTRICT sensor,
	const char *PASCAL_GOV_RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	sensor->scale_multiplier = SCALE_DECI;
	if (sensor->fd < 0) {
		LOGW("sensor: failed to open node %s err=%d", path, errno);
	}
}

void pascal_gov_sensor_init_battery(
	pascal_gov_battery_sensor *PASCAL_GOV_RESTRICT sensor,
	const char *PASCAL_GOV_RESTRICT path)
{
	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (sensor->fd < 0) {
		LOGW("sensor: failed to open node %s err=%d", path, errno);
	}
}

void pascal_gov_sensor_destroy_thermal(pascal_gov_thermal_sensor *sensor)
{
	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

void pascal_gov_sensor_destroy_battery(pascal_gov_battery_sensor *sensor)
{
	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

static inline int
read_temp_internal(pascal_gov_thermal_sensor *PASCAL_GOV_RESTRICT sensor,
		   float *PASCAL_GOV_RESTRICT out_temp, float fallback_val)
{
	if (sensor->fd < 0) {
		*out_temp = fallback_val;
		return -EBADF;
	}

	ssize_t bytes_read =
		pread(sensor->fd, sensor->buffer, sizeof(sensor->buffer), 0);

	if (bytes_read <= 0) {
		*out_temp = fallback_val;
		return -EIO;
	}

	bool has_digits;

	int32_t parsed_val = pascal_gov_parse_i32(
		sensor->buffer, (size_t)bytes_read, &has_digits);

	if (!has_digits) {
		*out_temp = fallback_val;
		return -EINVAL;
	}

	*out_temp = (float)parsed_val * sensor->scale_multiplier;

	return 0;
}

int pascal_gov_sensor_read_cpu_temp(
	pascal_gov_thermal_sensor *PASCAL_GOV_RESTRICT sensor,
	float *PASCAL_GOV_RESTRICT out_temp)
{
	return read_temp_internal(sensor, out_temp, FALLBACK_TEMP_CPU);
}

int pascal_gov_sensor_read_bat_temp(
	pascal_gov_thermal_sensor *PASCAL_GOV_RESTRICT sensor,
	float *PASCAL_GOV_RESTRICT out_temp)
{
	return read_temp_internal(sensor, out_temp, FALLBACK_TEMP_BAT);
}

int pascal_gov_sensor_read_battery(
	pascal_gov_battery_sensor *PASCAL_GOV_RESTRICT sensor,
	float *PASCAL_GOV_RESTRICT out_capacity)
{
	if (sensor->fd < 0) {
		*out_capacity = FALLBACK_BAT_CAPACITY;
		return -EBADF;
	}

	ssize_t bytes_read =
		pread(sensor->fd, sensor->buffer, sizeof(sensor->buffer), 0);

	if (bytes_read <= 0) {
		*out_capacity = FALLBACK_BAT_CAPACITY;
		return -EIO;
	}

	bool has_digits;

	int32_t parsed_val = pascal_gov_parse_i32(
		sensor->buffer, (size_t)bytes_read, &has_digits);

	*out_capacity =
		(int)has_digits ? (float)parsed_val : FALLBACK_BAT_CAPACITY;

	return 0;
}
