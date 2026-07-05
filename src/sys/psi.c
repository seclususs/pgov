// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "psi.h"
#include "pg/log.h"
#include "parser.h"
#include "pg/time.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int pg_psi_open_trg(const char *path, int32_t threshold_us, int32_t window_us)
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

	pos += pg_fmt_u32(threshold_us, buf + pos);
	buf[pos++] = ' ';

	pos += pg_fmt_u32(window_us, buf + pos);
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

void pg_psi_close_trg(int fd)
{
	if (fd >= 0)
		close(fd);
}

int pg_psi_recov(struct pg_psi_monitor *RESTRICT monitor,
		 const char *RESTRICT path)
{
	if (monitor->fd >= 0) {
		close(monitor->fd);
		monitor->fd = -1;
	}

	monitor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (monitor->fd < 0)
		return -errno;

	pg_kalman_reset(&monitor->filter_some);
	monitor->first_run = true;
	return 0;
}

void pg_psi_init(struct pg_psi_monitor *RESTRICT monitor,
		 const char *RESTRICT path,
		 const struct pg_kalman_cfg *RESTRICT cfg)
{
	monitor->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (monitor->fd < 0)
		LOGE("psi: failed to open node %s", path);

	clock_gettime(CLOCK_MONOTONIC, &monitor->last_read_ts);
	monitor->last_some_total = 0;
	monitor->first_run = true;
	pg_kalman_init(&monitor->filter_some, cfg);
}

void pg_psi_cleanup(struct pg_psi_monitor *monitor)
{
	if (monitor->fd >= 0) {
		close(monitor->fd);
		monitor->fd = -1;
	}
}

static inline int buffer_cmp(const uint8_t *RESTRICT buf,
			     const char *RESTRICT str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
		if (buf[i] != (uint8_t)str[i])
			return -1;

	return 0;
}

int pg_psi_read(struct pg_psi_monitor *RESTRICT monitor,
		struct pg_psi_data *RESTRICT data,
		const struct timespec *RESTRICT now)
{
	if (monitor->fd < 0) {
		LOGE("psi: invalid fd for read");
		return -EBADF;
	}

	ssize_t sz = pread(monitor->fd, monitor->buf, sizeof(monitor->buf), 0);
	if (sz <= 0) {
		LOGE("psi: read failed or empty err=%d", errno);
		return -EIO;
	}

	float dt_sec;
	float dt_calc;

	if (monitor->first_run) {
		dt_sec = 1.0F;
		dt_calc = 1000000.0F;
	} else {
		dt_sec = pg_dt_sec(&monitor->last_read_ts, now);
		if (dt_sec < 0.001F)
			dt_sec = 0.001F;

		dt_calc = dt_sec * 1000000.0F;
	}

	struct pg_psi_trend some_trend = { 0 };
	uint64_t cur_tot = monitor->last_some_total;

	size_t pos = 0;
	size_t len = (size_t)sz;

	bool found_some = false;

	while (pos < len) {
		if (pos + 5 <= len &&
		    buffer_cmp(&monitor->buf[pos], "some ", 5) == 0) {
			pos += 5;
			found_some = true;
			break;
		}

		while (pos < len && monitor->buf[pos] != '\n')
			pos++;

		pos++;
	}

	if (found_some) {
		while (pos < len && monitor->buf[pos] != '\n') {
			if (monitor->buf[pos] == ' ') {
				pos++;
				continue;
			}

			if (pos + 6 <= len &&
			    buffer_cmp(&monitor->buf[pos], "avg10=", 6) == 0) {
				if (LIKELY(!monitor->first_run)) {
					pos += 6;

					while (pos < len &&
					       monitor->buf[pos] != ' ' &&
					       monitor->buf[pos] != '\n')
						pos++;

					continue;
				}

				size_t next_pos;

				some_trend.avg10 = pg_parse_f32(
					monitor->buf, len, pos + 6, &next_pos);

				pos = next_pos;
				continue;
			}

			if (pos + 7 <= len &&
			    buffer_cmp(&monitor->buf[pos], "avg300=", 7) == 0) {
				size_t next_pos;

				some_trend.avg300 = pg_parse_f32(
					monitor->buf, len, pos + 7, &next_pos);

				pos = next_pos;
				continue;
			}

			if (pos + 6 <= len &&
			    buffer_cmp(&monitor->buf[pos], "total=", 6) == 0) {
				size_t next_pos;

				uint64_t temp_total = pg_parse_u64(
					monitor->buf, len, pos + 6, &next_pos);

				if (LIKELY(next_pos > pos + 6))
					cur_tot = temp_total;

				pos = next_pos;
				continue;
			}

			while (pos < len && monitor->buf[pos] != ' ' &&
			       monitor->buf[pos] != '\n')
				pos++;
		}
	}

	if (monitor->first_run) {
		some_trend.cur = some_trend.avg10;
		some_trend.vel = 0.0F;

		pg_kalman_reset(&monitor->filter_some);
		pg_kalman_update(&monitor->filter_some, some_trend.avg10, 1.0F);

		monitor->first_run = false;
	} else {
		float delta = 0.0F;

		if (LIKELY(cur_tot > monitor->last_some_total))
			delta = (float)(cur_tot - monitor->last_some_total);

		float inv_dt = 1.0F / dt_calc;
		float raw_some = delta * inv_dt * 100.0F;

		some_trend.cur = pg_kalman_update(&monitor->filter_some,
						  raw_some, dt_sec);

		some_trend.vel = monitor->filter_some.x_vel;
		some_trend.nis = monitor->filter_some.nis;
	}

	monitor->last_read_ts = *now;
	monitor->last_some_total = cur_tot;

	data->some = some_trend;

	return 0;
}
