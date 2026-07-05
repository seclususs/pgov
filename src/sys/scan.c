// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "scan.h"
#include "compiler.h"
#include "str.h"
#include "paths.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static const char *const PRIORITY[] = {
	"cpu",
	"cpu-1-0-usr",
	"cpu-1-1-usr",
	"cpu-1-2-usr",
	"cpu-1-3-usr",
	"cpu-0-0-usr",
	"cpu-0-1-usr",
	"cpu0_thermal",
	"cpu1_thermal",
	"mtktscpu",
	"mtk_ts_cpu",
	"mtkts_cpu",
	"thermal-cpuss-0",
	"thermal-cpuss-1",
	"exynos_thermal",
	"exynos_dev_thermal",
	"hisi_thermal",
	"mtktsAP",
	"mtk_ts_ap",
	"ap_cdev",
	"ap_thermal",
	"soc_thermal",
	"soc-thermal",
	"cpu_thermal",
	"cpu-thermal",
	"tsens_tz_sensor10",
	"tsens_tz_sensor5",
	"tsens_tz_sensor0",
	"big-core",
	"mid-core",
	"little-core",
};

static const char *const BLACKLIST[] = {
	"battery", "bms",  "bat",  "charger",  "usb",	 "pa_therm", "pa-therm",
	"modem",   "wifi", "wlan", "gpu",      "camera", "flash",    "led",
	"pmic",	   "buck", "ldo",  "xo_therm", "quiet",	 "backlight"
};

int pg_scan_thermal_zone(char *path, size_t len)
{
	DIR *dir = opendir(PG_PATH_THERMAL_BASE);
	if (!dir) {
		path[0] = '\0';
		return -1;
	}

	struct dirent *entry;
	char type_path[256];
	char buf[64];

	struct {
		char name[64];
		char folder[64];
	} zones[256];

	size_t nr_zones = 0;

	while ((entry = readdir(dir)) != NULL && nr_zones < 256) {
		if (!pg_str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		pg_str_build_path(type_path, sizeof(type_path),
				  PG_PATH_THERMAL_BASE, entry->d_name, "type");

		int fd = open(type_path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		ssize_t bytes = read(fd, buf, sizeof(buf) - 1);

		close(fd);

		if (bytes <= 0)
			continue;

		buf[bytes] = '\0';

		for (ssize_t i = 0; i < bytes; ++i) {
			if (buf[i] == '\n' || buf[i] == '\r') {
				buf[i] = '\0';
				break;
			}
		}

		strncpy(zones[nr_zones].name, buf, 63);
		zones[nr_zones].name[63] = '\0';

		strncpy(zones[nr_zones].folder, entry->d_name, 63);
		zones[nr_zones].folder[63] = '\0';

		nr_zones++;
	}

	closedir(dir);

	for (size_t i = 0; i < ARRAY_SIZE(PRIORITY); ++i) {
		for (size_t z = 0; z < nr_zones; ++z) {
			if (strcmp(zones[z].name, PRIORITY[i]) == 0) {
				pg_str_build_path(path, len,
						  PG_PATH_THERMAL_BASE,
						  zones[z].folder, "temp");
				return 0;
			}
		}
	}

	for (size_t z = 0; z < nr_zones; ++z) {
		char lower_name[64];
		strcpy(lower_name, zones[z].name);
		pg_str_to_lower(lower_name);

		bool is_cpu = pg_str_contains(lower_name, "cpu");

		if (!is_cpu)
			is_cpu = pg_str_contains(lower_name, "soc");

		if (!is_cpu)
			is_cpu = pg_str_contains(lower_name, "cluster");

		if (!is_cpu)
			is_cpu = pg_str_contains(lower_name, "ap");

		if (!is_cpu)
			continue;

		bool is_safe = true;

		for (size_t i = 0; i < ARRAY_SIZE(BLACKLIST); ++i) {
			if (pg_str_contains(lower_name, BLACKLIST[i])) {
				is_safe = false;
				break;
			}
		}

		if (is_cpu && is_safe) {
			pg_str_build_path(path, len, PG_PATH_THERMAL_BASE,
					  zones[z].folder, "temp");
			return 0;
		}
	}

	path[0] = '\0';
	return -1;
}
