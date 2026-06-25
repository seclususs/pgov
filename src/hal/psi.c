// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pascal_gov/psi.h"
#include "daemon/logger.h"
#include "pascal_gov/parser.h"
#include "pascal_gov/time.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int pascal_gov_psi_register_trigger(const char *path, int32_t threshold_us,
				    int32_t window_us)
{
	int fd = open(path, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0) {
		int err = errno;
		LOGE("psi: failed to open trigger node %s err=%d", path, err);
		return -err;
	}

	char buf[64];
	buf[0] = 's';
	buf[1] = 'o';
	buf[2] = 'm';
	buf[3] = 'e';
	buf[4] = ' ';
	size_t pos = 5;

	pos += pascal_gov_fmt_u32(threshold_us, buf + pos);
	buf[pos++] = ' ';

	pos += pascal_gov_fmt_u32(window_us, buf + pos);
	buf[pos++] = '\n';

	if (write(fd, buf, pos) < 0) {
		int err = errno;
		LOGE("psi: failed to write trigger config err=%d", err);
		close(fd);
		return -err;
	}

	LOGD("psi: registered trigger %dus/%dus", threshold_us, window_us);
	return fd;
}

void pascal_gov_psi_unregister_trigger(int fd)
{
	if (fd >= 0) {
		close(fd);
	}
}

int pascal_gov_psi_recover(pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor,
			   const char *path)
{
	if (monitor->fd >= 0) {
		close(monitor->fd);
		monitor->fd = -1;
	}

	monitor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (monitor->fd < 0) {
		return -errno;
	}

	pascal_gov_kalman_reset(&monitor->filter_some);
	monitor->first_run = true;
	return 0;
}

void pascal_gov_psi_init(
	pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor, const char *path,
	const pascal_gov_kalman_config *PASCAL_GOV_RESTRICT config)
{
	monitor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (monitor->fd < 0)
		LOGE("psi: failed to open node %s", path);

	clock_gettime(CLOCK_MONOTONIC, &monitor->last_read_time);
	monitor->last_some_total = 0;
	monitor->first_run = true;
	pascal_gov_kalman_init(&monitor->filter_some, config);
}

void pascal_gov_psi_destroy(pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor)
{
	if (monitor->fd >= 0) {
		close(monitor->fd);
		monitor->fd = -1;
	}
}

static inline int buffer_cmp(const uint8_t *buf, const char *str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
		if (buf[i] != (uint8_t)str[i])
			return -1;

	return 0;
}

int pascal_gov_psi_read_state(
	pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor,
	pascal_gov_psi_data *PASCAL_GOV_RESTRICT out_data,
	const struct timespec *PASCAL_GOV_RESTRICT now)
{
	if (monitor->fd < 0) {
		LOGE("psi: invalid fd for read");
		return -EBADF;
	}

	ssize_t bytes_read =
		pread(monitor->fd, monitor->buffer, sizeof(monitor->buffer), 0);

	if (bytes_read <= 0) {
		LOGE("psi: read failed or empty err=%d", errno);
		return -EIO;
	}

	float dt_sec;
	float dt_calc;

	if (monitor->first_run) {
		dt_sec = 1.0F;
		dt_calc = 1000000.0F;
	} else {
		dt_sec = pascal_gov_dt_sec(&monitor->last_read_time, now);
		if (dt_sec < 0.001F)
			dt_sec = 0.001F;

		dt_calc = dt_sec * 1000000.0F;
	}

	pascal_gov_psi_trend some_trend = {0};
	uint64_t current_total = monitor->last_some_total;

	size_t pos = 0;
	size_t len = (size_t)bytes_read;

	bool found_some = false;

	while (pos < len) {
		if (pos + 5 <= len &&
		    buffer_cmp(&monitor->buffer[pos], "some ", 5) == 0) {
			pos += 5;
			found_some = true;
			break;
		}

		while (pos < len && monitor->buffer[pos] != '\n')
			pos++;

		pos++;
	}

	if (found_some) {
		while (pos < len && monitor->buffer[pos] != '\n') {
			if (monitor->buffer[pos] == ' ') {
				pos++;
				continue;
			}

			if (pos + 6 <= len && buffer_cmp(&monitor->buffer[pos],
							 "avg10=", 6) == 0) {
				if (PASCAL_GOV_LIKELY(!monitor->first_run)) {
					pos += 6;

					while (pos < len &&
					       monitor->buffer[pos] != ' ' &&
					       monitor->buffer[pos] != '\n')
						pos++;

					continue;
				}

				size_t next_pos;

				some_trend.avg10 = pascal_gov_parse_f32(
					monitor->buffer, len, pos + 6,
					&next_pos);

				pos = next_pos;
				continue;
			}

			if (pos + 7 <= len && buffer_cmp(&monitor->buffer[pos],
							 "avg300=", 7) == 0) {
				size_t next_pos;

				some_trend.avg300 = pascal_gov_parse_f32(
					monitor->buffer, len, pos + 7,
					&next_pos);

				pos = next_pos;
				continue;
			}

			if (pos + 6 <= len && buffer_cmp(&monitor->buffer[pos],
							 "total=", 6) == 0) {
				size_t next_pos;

				uint64_t temp_total = pascal_gov_parse_u64(
					monitor->buffer, len, pos + 6,
					&next_pos);

				if (PASCAL_GOV_LIKELY(next_pos > pos + 6))
					current_total = temp_total;

				pos = next_pos;
				continue;
			}

			while (pos < len && monitor->buffer[pos] != ' ' &&
			       monitor->buffer[pos] != '\n')
				pos++;
		}
	}

	if (monitor->first_run) {
		some_trend.current = some_trend.avg10;
		some_trend.velocity = 0.0F;

		pascal_gov_kalman_reset(&monitor->filter_some);
		pascal_gov_kalman_update(&monitor->filter_some,
					 some_trend.avg10, 1.0F);

		monitor->first_run = false;
	} else {
		float delta_some = 0.0F;

		if (PASCAL_GOV_LIKELY(current_total > monitor->last_some_total))
			delta_some = (float)(current_total -
					     monitor->last_some_total);

		float inv_dt_calc = 1.0F / dt_calc;
		float raw_some = delta_some * inv_dt_calc * 100.0F;

		some_trend.current = pascal_gov_kalman_update(
			&monitor->filter_some, raw_some, dt_sec);

		some_trend.velocity = monitor->filter_some.x_vel;
		some_trend.nis = monitor->filter_some.last_nis;
	}

	monitor->last_read_time = *now;
	monitor->last_some_total = current_total;

	out_data->some = some_trend;

	return 0;
}
