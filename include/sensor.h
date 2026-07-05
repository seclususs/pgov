// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SENSOR_H
#define PGOV_SENSOR_H

#include "compiler.h"
#include <stdint.h>

struct pg_temp_sensor {
	int fd;
	float scale;
	uint8_t buf[16];
};

struct pg_bat_sensor {
	int fd;
	uint8_t buf[16];
};

void pg_sensor_init_cpu_temp(struct pg_temp_sensor *RESTRICT sensor,
			     const char *RESTRICT path);

void pg_sensor_init_bat_temp(struct pg_temp_sensor *RESTRICT sensor,
			     const char *RESTRICT path);

void pg_sensor_temp_close(struct pg_temp_sensor *sensor);

void pg_sensor_init_bat_cap(struct pg_bat_sensor *RESTRICT sensor,
			    const char *RESTRICT path);

void pg_sensor_bat_close(struct pg_bat_sensor *sensor);

int pg_sensor_read_cpu_temp(struct pg_temp_sensor *RESTRICT sensor,
			    float *RESTRICT temp);

int pg_sensor_read_bat_temp(struct pg_temp_sensor *RESTRICT sensor,
			    float *RESTRICT temp);

int pg_sensor_read_bat_cap(struct pg_bat_sensor *RESTRICT sensor,
			   float *RESTRICT cap);

#endif // PGOV_SENSOR_H
