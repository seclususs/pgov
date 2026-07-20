// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "scan.h"
#include "compiler.h"
#include "str.h"
#include "paths.h"
#include <dirent.h>
#include <errno.h>
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

struct tz_info {
	char name[64];
	char folder[64];
};

static inline bool thermal_valid(const char *name)
{
	char l_name[64];
	strncpy(l_name, name, sizeof(l_name) - 1);
	l_name[sizeof(l_name) - 1] = '\0';
	pg_str_to_lower(l_name);

	bool cpu = (pg_str_contains(l_name, "cpu") ||
		    pg_str_contains(l_name, "soc") ||
		    pg_str_contains(l_name, "cluster") ||
		    pg_str_contains(l_name, "ap")) != 0;

	if (!cpu)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(BLACKLIST); ++i) {
		if (pg_str_contains(l_name, BLACKLIST[i]))
			return false;
	}

	return true;
}

static inline int find_priority(const struct tz_info *zones, size_t nr_zones,
				char *path, size_t len)
{
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

	return -ENOENT;
}

static inline int find_heuristic(const struct tz_info *zones, size_t nr_zones,
				 char *path, size_t len)
{
	for (size_t z = 0; z < nr_zones; ++z) {
		if (thermal_valid(zones[z].name)) {
			pg_str_build_path(path, len, PG_PATH_THERMAL_BASE,
					  zones[z].folder, "temp");
			return 0;
		}
	}

	return -ENOENT;
}

int pg_scan_thermal_zone(char *path, size_t len)
{
	DIR *dir = opendir(PG_PATH_THERMAL_BASE);
	if (!dir) {
		if (len > 0)
			path[0] = '\0';

		return -ENOENT;
	}

	struct dirent *entry;
	char type_path[256];
	char buf[64];
	struct tz_info zones[256];
	size_t nr_zones = 0;

	while ((entry = readdir(dir)) != NULL && nr_zones < 256) {
		if (!pg_str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		pg_str_build_path(type_path, sizeof(type_path),
				  PG_PATH_THERMAL_BASE, entry->d_name, "type");

		int fd = open(type_path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		ssize_t bytes;
		do {
			bytes = read(fd, buf, sizeof(buf) - 1);
		} while (bytes < 0 && errno == EINTR);

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

	if (find_priority(zones, nr_zones, path, len) == 0)
		return 0;

	if (find_heuristic(zones, nr_zones, path, len) == 0)
		return 0;

	if (len > 0)
		path[0] = '\0';

	return -ENODEV;
}

int pg_scan_backlight(char *path, size_t len)
{
	DIR *dir = opendir(PG_PATH_BACKLIGHT_BASE);
	if (!dir) {
		if (len > 0)
			path[0] = '\0';

		return -ENOENT;
	}

	struct dirent *entry;
	bool found = false;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		pg_str_build_path(path, len, PG_PATH_BACKLIGHT_BASE,
				  entry->d_name, "brightness");

		int fd = open(path, O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			close(fd);
			found = true;
			break;
		}
	}

	closedir(dir);

	if (!found) {
		if (len > 0)
			path[0] = '\0';

		return -ENODEV;
	}

	return 0;
}

static int trip_point(const char *name, char *path, size_t len)
{
	char type[256];
	char buf[64];
	char file[32];
	char temp[32];

	for (size_t i = 0; i <= 9; ++i) {
		file[0] = 't';
		file[1] = 'r';
		file[2] = 'i';
		file[3] = 'p';
		file[4] = '_';
		file[5] = 'p';
		file[6] = 'o';
		file[7] = 'i';
		file[8] = 'n';
		file[9] = 't';
		file[10] = '_';
		file[11] = (char)('0' + i);
		file[12] = '_';
		file[13] = 't';
		file[14] = 'y';
		file[15] = 'p';
		file[16] = 'e';
		file[17] = '\0';

		pg_str_build_path(type, sizeof(type), PG_PATH_THERMAL_BASE,
				  name, file);

		int fd = open(type, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		ssize_t bytes;
		do {
			bytes = read(fd, buf, sizeof(buf) - 1);
		} while (bytes < 0 && errno == EINTR);

		close(fd);

		if (bytes <= 0)
			continue;

		buf[bytes] = '\0';

		if (pg_str_contains(buf, "passive") ||
		    pg_str_contains(buf, "critical")) {
			temp[0] = 't';
			temp[1] = 'r';
			temp[2] = 'i';
			temp[3] = 'p';
			temp[4] = '_';
			temp[5] = 'p';
			temp[6] = 'o';
			temp[7] = 'i';
			temp[8] = 'n';
			temp[9] = 't';
			temp[10] = '_';
			temp[11] = (char)('0' + i);
			temp[12] = '_';
			temp[13] = 't';
			temp[14] = 'e';
			temp[15] = 'm';
			temp[16] = 'p';
			temp[17] = '\0';

			pg_str_build_path(path, len, PG_PATH_THERMAL_BASE, name,
					  temp);
			return 0;
		}
	}

	return -ENOENT;
}

int pg_scan_trip_point(char *path, size_t len)
{
	DIR *dir = opendir(PG_PATH_THERMAL_BASE);
	struct dirent *entry;
	int ret = -ENODEV;

	if (!dir) {
		if (len > 0)
			path[0] = '\0';

		return -ENOENT;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!pg_str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		if (!thermal_valid(entry->d_name))
			continue;

		if (trip_point(entry->d_name, path, len) == 0) {
			ret = 0;
			goto out;
		}
	}

	if (len > 0)
		path[0] = '\0';

out:
	closedir(dir);
	return ret;
}
