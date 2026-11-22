// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SENSOR_H
#define PGOV_SENSOR_H

#include "pgov/math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSOR_BACKLIGHT_MAX 4
#define SENSOR_PATH_LEN 128

struct sensor_temp {
	int fd;
	q16_t scale;
	bool calibrated;
	uint8_t buf[16];
};

struct sensor_battery {
	int fd;
	uint8_t buf[16];
};

struct sensor_backlight {
	int fd[SENSOR_BACKLIGHT_MAX];
	uint8_t count;
	uint8_t buf[16];
};

/*
 * Scan sysfs thermal zones and select the best matching CPU or SoC sensor
 *
 * Returns 0 on success, or a negative error code (-EINVAL, -ENOENT, -ENODEV)
 */
int sensor_scan_thermal_zone(char *path, size_t len);

/*
 * Scan backlight and LED brightness nodes, returning the count of usable nodes
 *
 * Returns count on success, or a negative error code (-EINVAL, -ENODEV)
 */
int sensor_scan_backlight(char (*paths)[SENSOR_PATH_LEN], size_t max);

void sensor_init_cpu_temp(struct sensor_temp *sensor, const char *path);

void sensor_init_battery_temp(struct sensor_temp *sensor,
			      const char *const *paths);

void sensor_close_temp(struct sensor_temp *sensor);

void sensor_init_battery(struct sensor_battery *sensor,
			 const char *const *paths);

void sensor_close_battery(struct sensor_battery *sensor);

void sensor_init_backlight(struct sensor_backlight *sensor, const char *path);

void sensor_close_backlight(struct sensor_backlight *sensor);

int sensor_read_cpu_temp(struct sensor_temp *sensor, q16_t *temp);

int sensor_read_battery_temp(struct sensor_temp *sensor, q16_t *temp);

int sensor_read_battery_capacity(struct sensor_battery *sensor,
				 q16_t *capacity);

int sensor_read_backlight(struct sensor_backlight *sensor, int32_t *brightness);

/*
 * Find thermal trip point and return threshold temperature in Q16
 *
 * Returns trip temperature on success, or fallback on error
 */
q16_t sensor_get_trip_temp(q16_t fallback);

#endif // PGOV_SENSOR_H
