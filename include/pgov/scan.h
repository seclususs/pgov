// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_SCAN_H
#define PGOV_SCAN_H

#include <stddef.h>

#define PG_BL_PATH_LEN 128

int pg_scan_thermal_zone(char *path, size_t len);
int pg_scan_backlight(char (*paths)[PG_BL_PATH_LEN], size_t max);
int pg_scan_trip_point(char *path, size_t len);

#endif // PGOV_SCAN_H
