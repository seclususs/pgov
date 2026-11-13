// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/scan.h"
#include "pgov/compiler.h"
#include "pgov/str.h"
#include "pgov/path.h"
#include "pgov/parser.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
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
	"battery",  "bms",	 "bat",	    "charger", "usb", "pa_therm",
	"pa-therm", "modem",	 "wifi",    "wlan",    "gpu", "camera",
	"flash",    "led",	 "pmic",    "buck",    "ldo", "xo_therm",
	"quiet",    "backlight", "adapter",
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

static ssize_t read_sysfs_str(const char *path, char *buf, size_t len)
{
	if (UNLIKELY(!path || !buf || len == 0))
		return -1;

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	ssize_t bytes;
	do {
		bytes = read(fd, buf, len - 1);
	} while (bytes < 0 && errno == EINTR);

	close(fd);

	if (bytes <= 0)
		return -1;

	buf[bytes] = '\0';
	for (ssize_t i = 0; i < bytes; ++i) {
		if (buf[i] == '\n' || buf[i] == '\r') {
			buf[i] = '\0';
			bytes = i;
			break;
		}
	}

	return bytes;
}

static bool type_valid(const char *name)
{
	char type_path[256];
	char buf[64];

	pg_str_build_path(type_path, sizeof(type_path), PATH_THERMAL_BASE, name,
			  "type");

	if (read_sysfs_str(type_path, buf, sizeof(buf)) <= 0)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(PRIORITY); ++i) {
		if (strcmp(buf, PRIORITY[i]) == 0)
			return true;
	}

	return thermal_valid(buf);
}

static inline int find_priority(const struct tz_info *zones, size_t nr_zones,
				char *path, size_t len)
{
	for (size_t i = 0; i < ARRAY_SIZE(PRIORITY); ++i) {
		for (size_t z = 0; z < nr_zones; ++z) {
			if (strcmp(zones[z].name, PRIORITY[i]) == 0) {
				pg_str_build_path(path, len, PATH_THERMAL_BASE,
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
			pg_str_build_path(path, len, PATH_THERMAL_BASE,
					  zones[z].folder, "temp");
			return 0;
		}
	}

	return -ENOENT;
}

int pg_scan_thermal_zone(char *path, size_t len)
{
	if (UNLIKELY(!path || len == 0))
		return -EINVAL;

	DIR *dir = opendir(PATH_THERMAL_BASE);
	if (!dir) {
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
				  PATH_THERMAL_BASE, entry->d_name, "type");

		if (read_sysfs_str(type_path, buf, sizeof(buf)) <= 0)
			continue;

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

	path[0] = '\0';
	return -ENODEV;
}

static void scan_brightness(const char *base, const char *sub_filter,
			    char (*paths)[BL_PATH_LEN], size_t *n, size_t max)
{
	DIR *dir = opendir(base);
	if (!dir)
		return;

	struct dirent *entry;
	while (*n < max && (entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		if (sub_filter && !pg_str_contains(entry->d_name, sub_filter))
			continue;

		pg_str_build_path(paths[*n], BL_PATH_LEN, base, entry->d_name,
				  "brightness");

		int fd = open(paths[*n], O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		close(fd);
		(*n)++;
	}

	closedir(dir);
}

int pg_scan_backlight(char (*paths)[BL_PATH_LEN], size_t max)
{
	if (UNLIKELY(!paths || max == 0))
		return -EINVAL;

	size_t n = 0;
	scan_brightness(PATH_BACKLIGHT_BASE, NULL, paths, &n, max);
	if (n == 0)
		scan_brightness(PATH_LEDS_BASE, "backlight", paths, &n, max);

	if (n == 0)
		return -ENODEV;

	return (int)n;
}

static inline void format_trip_node(char *RESTRICT buf, size_t i,
				    const char *RESTRICT suffix)
{
	const char prefix[] = "trip_point_";
	size_t p = 0;

	while (prefix[p] != '\0') {
		buf[p] = prefix[p];
		p++;
	}

	buf[p++] = (char)('0' + i);
	buf[p++] = '_';

	size_t s = 0;
	while (suffix[s] != '\0') {
		buf[p++] = suffix[s++];
	}

	buf[p] = '\0';
}

static int trip_point(const char *name, char *path, size_t len)
{
	char file[32];
	char node_path[256];
	char buf[64];
	char best_path[256] = "";
	int best_score = 0;
	int32_t best_temp = INT32_MAX;

	for (size_t i = 0; i <= 9; ++i) {
		format_trip_node(file, i, "type");
		pg_str_build_path(node_path, sizeof(node_path),
				  PATH_THERMAL_BASE, name, file);

		if (read_sysfs_str(node_path, buf, sizeof(buf)) <= 0)
			continue;

		int score = 0;

		if (pg_str_contains(buf, "passive"))
			score = 3;

		else if (pg_str_contains(buf, "active"))
			score = 2;

		else if (pg_str_contains(buf, "critical"))
			score = 1;

		if (score == 0)
			continue;

		format_trip_node(file, i, "temp");
		pg_str_build_path(node_path, sizeof(node_path),
				  PATH_THERMAL_BASE, name, file);

		ssize_t bytes = read_sysfs_str(node_path, buf, sizeof(buf));
		if (bytes <= 0)
			continue;

		bool valid = false;
		int32_t cur_temp = pg_parse_i32((const uint8_t *)buf,
						(size_t)bytes, &valid);
		if (!valid || cur_temp < 40000)
			continue;

		if (score > best_score ||
		    (score == best_score && cur_temp < best_temp)) {
			best_score = score;
			best_temp = cur_temp;
			strncpy(best_path, node_path, sizeof(best_path) - 1);
			best_path[sizeof(best_path) - 1] = '\0';
		}
	}

	if (best_score > 0 && best_path[0] != '\0') {
		strncpy(path, best_path, len - 1);
		path[len - 1] = '\0';
		return 0;
	}

	return -ENOENT;
}

int pg_scan_trip_point(char *path, size_t len)
{
	if (UNLIKELY(!path || len == 0))
		return -EINVAL;

	DIR *dir = opendir(PATH_THERMAL_BASE);
	struct dirent *entry;
	int ret = -ENODEV;

	if (!dir) {
		path[0] = '\0';
		return -ENOENT;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!pg_str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		if (!type_valid(entry->d_name))
			continue;

		if (trip_point(entry->d_name, path, len) == 0) {
			ret = 0;
			goto out;
		}
	}

	path[0] = '\0';

out:
	closedir(dir);
	return ret;
}
