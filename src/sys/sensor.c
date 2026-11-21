// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/sensor.h"
#include "pgov/compiler.h"
#include "pgov/log.h"
#include "pgov/node.h"
#include "pgov/parser.h"
#include "pgov/str.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define THERMAL "/sys/class/thermal"
#define BACKLIGHT "/sys/class/backlight"
#define LEDS "/sys/class/leds"

#define SCALE_MILLI 1000
#define SCALE_DECI 10

#define FALLBACK_TEMP_CPU INT_TO_Q16(65)
#define FALLBACK_TEMP_BATTERY INT_TO_Q16(35)
#define FALLBACK_BATTERY_CAPACITY INT_TO_Q16(100)
#define FALLBACK_BACKLIGHT 0

static const char *const THERMAL_PRIORITY[] = {
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

static const char *const THERMAL_BLACKLIST[] = {
	"battery",  "bms",	 "bat",	    "charger", "usb", "pa_therm",
	"pa-therm", "modem",	 "wifi",    "wlan",    "gpu", "camera",
	"flash",    "led",	 "pmic",    "buck",    "ldo", "xo_therm",
	"quiet",    "backlight", "adapter",
};

struct thermal_zone_info {
	char name[64];
	char folder[64];
};

static inline bool check_thermal_valid(const char *name)
{
	char lower_name[64];
	bool cpu_match;

	strncpy(lower_name, name, sizeof(lower_name) - 1);
	lower_name[sizeof(lower_name) - 1] = '\0';
	str_to_lower(lower_name);

	cpu_match = (str_contains(lower_name, "cpu") ||
		     str_contains(lower_name, "soc") ||
		     str_contains(lower_name, "cluster") ||
		     str_contains(lower_name, "ap")) != 0;

	if (!cpu_match)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(THERMAL_BLACKLIST); ++i) {
		if (str_contains(lower_name, THERMAL_BLACKLIST[i]))
			return false;
	}

	return true;
}

static bool check_thermal_type(const char *name)
{
	char type_path[256];
	char buf[64];

	str_build_path(type_path, sizeof(type_path), THERMAL, name, "type");

	if (node_read_str(type_path, buf, sizeof(buf)) <= 0)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(THERMAL_PRIORITY); ++i) {
		if (strcmp(buf, THERMAL_PRIORITY[i]) == 0)
			return true;
	}

	return check_thermal_valid(buf);
}

static inline int find_priority_zone(const struct thermal_zone_info *zones,
				     size_t zone_count, char *path, size_t len)
{
	for (size_t i = 0; i < ARRAY_SIZE(THERMAL_PRIORITY); ++i) {
		for (size_t z = 0; z < zone_count; ++z) {
			if (strcmp(zones[z].name, THERMAL_PRIORITY[i]) == 0) {
				str_build_path(path, len, THERMAL,
					       zones[z].folder, "temp");
				return 0;
			}
		}
	}

	return -ENOENT;
}

static inline int find_heuristic_zone(const struct thermal_zone_info *zones,
				      size_t zone_count, char *path, size_t len)
{
	for (size_t z = 0; z < zone_count; ++z) {
		if (check_thermal_valid(zones[z].name)) {
			str_build_path(path, len, THERMAL, zones[z].folder,
				       "temp");
			return 0;
		}
	}

	return -ENOENT;
}

int sensor_scan_thermal_zone(char *path, size_t len)
{
	struct thermal_zone_info zones[256];
	struct dirent *entry;
	size_t zone_count = 0;
	char type_path[256];
	char buf[64];
	DIR *dir;

	if (UNLIKELY(!path || len == 0))
		return -EINVAL;

	dir = opendir(THERMAL);
	if (!dir) {
		path[0] = '\0';
		return -ENOENT;
	}

	while ((entry = readdir(dir)) != NULL && zone_count < 256) {
		if (!str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		str_build_path(type_path, sizeof(type_path), THERMAL,
			       entry->d_name, "type");

		if (node_read_str(type_path, buf, sizeof(buf)) <= 0)
			continue;

		strncpy(zones[zone_count].name, buf, 63);
		zones[zone_count].name[63] = '\0';

		strncpy(zones[zone_count].folder, entry->d_name, 63);
		zones[zone_count].folder[63] = '\0';

		zone_count++;
	}

	closedir(dir);

	if (find_priority_zone(zones, zone_count, path, len) == 0)
		return 0;

	if (find_heuristic_zone(zones, zone_count, path, len) == 0)
		return 0;

	path[0] = '\0';
	return -ENODEV;
}

static void scan_brightness_nodes(const char *base, const char *sub_filter,
				  char (*paths)[SENSOR_PATH_LEN], size_t *count,
				  size_t max)
{
	struct dirent *entry;
	DIR *dir;
	int fd;

	dir = opendir(base);
	if (!dir)
		return;

	while (*count < max && (entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		if (sub_filter && !str_contains(entry->d_name, sub_filter))
			continue;

		str_build_path(paths[*count], SENSOR_PATH_LEN, base,
			       entry->d_name, "brightness");

		fd = open(paths[*count], O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		close(fd);
		(*count)++;
	}

	closedir(dir);
}

int sensor_scan_backlight(char (*paths)[SENSOR_PATH_LEN], size_t max)
{
	size_t count = 0;

	if (UNLIKELY(!paths || max == 0))
		return -EINVAL;

	scan_brightness_nodes(BACKLIGHT, NULL, paths, &count, max);
	if (count == 0)
		scan_brightness_nodes(LEDS, "backlight", paths, &count, max);

	if (count == 0)
		return -ENODEV;

	return (int)count;
}

static inline void format_trip_node_path(char *RESTRICT buf, size_t index,
					 const char *RESTRICT suffix)
{
	const char prefix[] = "trip_point_";
	size_t pos = 0;
	size_t s = 0;

	while (prefix[pos] != '\0') {
		buf[pos] = prefix[pos];
		pos++;
	}

	buf[pos++] = (char)('0' + index);
	buf[pos++] = '_';

	while (suffix[s] != '\0')
		buf[pos++] = suffix[s++];

	buf[pos] = '\0';
}

static int find_best_trip_point(const char *name, char *path, size_t len)
{
	char best_path[256] = "";
	char node_path[256];
	char file[32];
	char buf[64];
	int32_t best_temp = INT32_MAX;
	int best_score = 0;

	for (size_t i = 0; i <= 9; ++i) {
		ssize_t res;
		int32_t cur_temp;
		bool valid;
		int score = 0;

		format_trip_node_path(file, i, "type");
		str_build_path(node_path, sizeof(node_path), THERMAL, name,
			       file);

		if (node_read_str(node_path, buf, sizeof(buf)) <= 0)
			continue;

		if (str_contains(buf, "passive"))
			score = 3;

		else if (str_contains(buf, "active"))
			score = 2;

		else if (str_contains(buf, "critical"))
			score = 1;

		if (score == 0)
			continue;

		format_trip_node_path(file, i, "temp");
		str_build_path(node_path, sizeof(node_path), THERMAL, name,
			       file);

		res = node_read_str(node_path, buf, sizeof(buf));
		if (res <= 0)
			continue;

		valid = false;
		cur_temp = parse_i32((const uint8_t *)buf, (size_t)res, &valid);

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

static int scan_trip_point_node(char *path, size_t len)
{
	struct dirent *entry;
	int ret = -ENODEV;
	DIR *dir;

	if (UNLIKELY(!path || len == 0))
		return -EINVAL;

	dir = opendir(THERMAL);
	if (!dir) {
		path[0] = '\0';
		return -ENOENT;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (!str_has_prefix(entry->d_name, "thermal_zone"))
			continue;

		if (!check_thermal_type(entry->d_name))
			continue;

		if (find_best_trip_point(entry->d_name, path, len) == 0) {
			ret = 0;
			goto out;
		}
	}

	path[0] = '\0';

out:
	closedir(dir);
	return ret;
}

void sensor_init_cpu_temp(struct sensor_temp *sensor, const char *path)
{
	char auto_path[256];

	if (UNLIKELY(!sensor))
		return;

	sensor->fd = -1;
	sensor->scale = SCALE_MILLI;

	if (!path || path[0] == '\0') {
		if (sensor_scan_thermal_zone(auto_path, sizeof(auto_path)) == 0)
			path = auto_path;
		else
			return;
	}

	sensor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (sensor->fd < 0)
		LOGW("sensor: failed to open node %s err=%d", path, errno);
}

void sensor_init_battery_temp(struct sensor_temp *sensor,
			      const char *const *paths)
{
	int fd;

	if (UNLIKELY(!sensor))
		return;

	sensor->fd = -1;
	sensor->scale = SCALE_DECI;

	if (UNLIKELY(!paths))
		return;

	for (size_t i = 0; paths[i] != NULL; i++) {
		fd = open(paths[i], O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			sensor->fd = fd;
			LOGI("sensor: mapped battery temp to %s", paths[i]);
			return;
		}
	}

	LOGW("sensor: no usable battery temp node");
}

void sensor_close_temp(struct sensor_temp *sensor)
{
	if (UNLIKELY(!sensor))
		return;

	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

void sensor_init_battery(struct sensor_battery *sensor,
			 const char *const *paths)
{
	int fd;

	if (UNLIKELY(!sensor))
		return;

	sensor->fd = -1;

	if (UNLIKELY(!paths))
		return;

	for (size_t i = 0; paths[i] != NULL; i++) {
		fd = open(paths[i], O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			sensor->fd = fd;
			LOGI("sensor: mapped battery capacity to %s", paths[i]);
			return;
		}
	}

	LOGW("sensor: no usable battery capacity node");
}

void sensor_close_battery(struct sensor_battery *sensor)
{
	if (UNLIKELY(!sensor))
		return;

	if (sensor->fd >= 0) {
		close(sensor->fd);
		sensor->fd = -1;
	}
}

void sensor_init_backlight(struct sensor_backlight *sensor, const char *path)
{
	int fd;

	if (UNLIKELY(!sensor || !path || path[0] == '\0'))
		return;

	if (sensor->count >= SENSOR_BACKLIGHT_MAX)
		return;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		LOGW("sensor: failed to open node %s err=%d", path, errno);
		return;
	}

	sensor->fd[sensor->count++] = fd;
}

void sensor_close_backlight(struct sensor_backlight *sensor)
{
	if (UNLIKELY(!sensor))
		return;

	for (uint8_t i = 0; i < sensor->count; i++) {
		if (sensor->fd[i] >= 0) {
			close(sensor->fd[i]);
			sensor->fd[i] = -1;
		}
	}

	sensor->count = 0;
}

static inline int read_temperature(struct sensor_temp *sensor, q16_t *temp,
				   q16_t fallback)
{
	ssize_t res;
	q32_t raw_q32;
	int32_t val;
	bool valid;

	if (UNLIKELY(!sensor || !temp))
		return -EINVAL;

	if (sensor->fd < 0) {
		*temp = fallback;
		return -EBADF;
	}

	do {
		res = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	} while (res < 0 && errno == EINTR);

	if (res <= 0) {
		*temp = fallback;
		return -EIO;
	}

	val = parse_i32(sensor->buf, (size_t)res, &valid);
	if (!valid) {
		*temp = fallback;
		return -EINVAL;
	}

	if (val >= 10000 && val <= 150000)
		sensor->scale = SCALE_MILLI;

	else if (val >= 100 && val <= 1500)
		sensor->scale = SCALE_DECI;

	else if (val >= -30 && val <= 150)
		sensor->scale = 1;

	if (UNLIKELY(sensor->scale <= 0)) {
		*temp = fallback;
		return -EINVAL;
	}

	raw_q32 = ((q32_t)val * Q16_ONE) / sensor->scale;

	if (UNLIKELY(raw_q32 < (q32_t)INT_TO_Q16(-30) ||
		     raw_q32 > (q32_t)INT_TO_Q16(150))) {
		*temp = fallback;
		return -ERANGE;
	}

	*temp = (q16_t)raw_q32;
	return 0;
}

int sensor_read_cpu_temp(struct sensor_temp *sensor, q16_t *temp)
{
	return read_temperature(sensor, temp, FALLBACK_TEMP_CPU);
}

int sensor_read_battery_temp(struct sensor_temp *sensor, q16_t *temp)
{
	return read_temperature(sensor, temp, FALLBACK_TEMP_BATTERY);
}

int sensor_read_battery_capacity(struct sensor_battery *sensor, q16_t *capacity)
{
	ssize_t res;
	int32_t val;
	bool valid;

	if (UNLIKELY(!sensor || !capacity))
		return -EINVAL;

	if (sensor->fd < 0) {
		*capacity = FALLBACK_BATTERY_CAPACITY;
		return -EBADF;
	}

	do {
		res = pread(sensor->fd, sensor->buf, sizeof(sensor->buf), 0);
	} while (res < 0 && errno == EINTR);

	if (res <= 0) {
		*capacity = FALLBACK_BATTERY_CAPACITY;
		return -EIO;
	}

	val = parse_i32(sensor->buf, (size_t)res, &valid);
	if (UNLIKELY(!valid || val < 0 || val > 100)) {
		*capacity = FALLBACK_BATTERY_CAPACITY;
		return -EINVAL;
	}

	*capacity = INT_TO_Q16(val);
	return 0;
}

int sensor_read_backlight(struct sensor_backlight *sensor, int32_t *brightness)
{
	int32_t best = FALLBACK_BACKLIGHT;
	bool any = false;

	if (UNLIKELY(!sensor || !brightness))
		return -EINVAL;

	if (sensor->count == 0) {
		*brightness = FALLBACK_BACKLIGHT;
		return -EBADF;
	}

	for (uint8_t i = 0; i < sensor->count; i++) {
		ssize_t res;
		int32_t val;
		bool valid;

		do {
			res = pread(sensor->fd[i], sensor->buf,
				    sizeof(sensor->buf), 0);
		} while (res < 0 && errno == EINTR);

		if (res <= 0)
			continue;

		val = parse_i32(sensor->buf, (size_t)res, &valid);
		if (UNLIKELY(!valid))
			continue;

		any = true;
		if (val > best)
			best = val;
	}

	*brightness = best;
	return any ? 0 : -EIO;
}

q16_t sensor_get_trip_temp(q16_t fallback)
{
	char path[256];
	int32_t val;
	q16_t temp;

	if (scan_trip_point_node(path, sizeof(path)) != 0)
		return fallback;

	if (node_read_i32(path, &val) != 0)
		return fallback;

	temp = (q16_t)(((q32_t)val * Q16_ONE) / 1000);
	if (temp < INT_TO_Q16(40) || temp > INT_TO_Q16(115))
		return fallback;

	return temp;
}
