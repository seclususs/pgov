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
	int err;
	int fd = open(path, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0) {
		err = errno;
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
		err = errno;
		LOGE("psi: failed to write trigger config err=%d", err);
		goto out_err;
	}

	LOGD("psi: registered trigger %dus/%dus", threshold_us, window_us);
	return fd;

out_err:
	close(fd);
	return -err;
}

void pg_psi_close_trg(int fd)
{
	if (fd >= 0)
		close(fd);
}

int pg_psi_recov(struct pg_psi_monitor *RESTRICT mon, const char *RESTRICT path)
{
	if (mon->fd >= 0) {
		close(mon->fd);
		mon->fd = -1;
	}

	mon->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (mon->fd < 0)
		return -errno;

	pg_kalman_reset(&mon->filter);
	mon->first_run = true;
	return 0;
}

void pg_psi_init(struct pg_psi_monitor *RESTRICT mon, const char *RESTRICT path)
{
	mon->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (mon->fd < 0)
		LOGE("psi: failed to open node %s", path);

	clock_gettime(CLOCK_MONOTONIC, &mon->last_read_ts);
	mon->last_tot = 0;
	mon->first_run = true;
	pg_kalman_init(&mon->filter);
}

void pg_psi_cleanup(struct pg_psi_monitor *mon)
{
	if (mon->fd >= 0) {
		close(mon->fd);
		mon->fd = -1;
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

static inline size_t parse(const uint8_t *RESTRICT buf, size_t len, size_t pos,
			   bool first_run, struct pg_psi_trend *RESTRICT trend,
			   uint64_t *RESTRICT cur_tot)
{
	while (pos < len && buf[pos] != '\n') {
		if (buf[pos] == ' ') {
			pos++;
			continue;
		}

		if (pos + 6 <= len && buffer_cmp(&buf[pos], "avg10=", 6) == 0) {
			if (LIKELY(!first_run)) {
				pos += 6;
				while (pos < len && buf[pos] != ' ' &&
				       buf[pos] != '\n')
					pos++;

				continue;
			}

			size_t n_pos;
			trend->avg10 = pg_parse_q16(buf, len, pos + 6, &n_pos);
			pos = n_pos;
			continue;
		}

		if (pos + 7 <= len &&
		    buffer_cmp(&buf[pos], "avg300=", 7) == 0) {
			size_t n_pos;
			trend->avg300 = pg_parse_q16(buf, len, pos + 7, &n_pos);
			pos = n_pos;
			continue;
		}

		if (pos + 6 <= len && buffer_cmp(&buf[pos], "total=", 6) == 0) {
			size_t n_pos;
			uint64_t tmp_tot =
				pg_parse_u64(buf, len, pos + 6, &n_pos);

			if (LIKELY(n_pos > pos + 6))
				*cur_tot = tmp_tot;

			pos = n_pos;
			continue;
		}

		while (pos < len && buf[pos] != ' ' && buf[pos] != '\n')
			pos++;
	}

	return pos;
}

static inline void calc_kalman(struct pg_psi_monitor *RESTRICT mon,
			       struct pg_psi_trend *RESTRICT trend,
			       uint64_t cur_tot, q16_t dt_sec)
{
	if (mon->first_run) {
		trend->cur = trend->avg10;
		trend->vel = 0;

		pg_kalman_reset(&mon->filter);
		pg_kalman_update(&mon->filter, trend->avg10, Q16_ONE);
		mon->first_run = false;
	} else {
		q16_t r_some = 0;
		if (LIKELY(cur_tot > mon->last_tot)) {
			uint64_t delta = cur_tot - mon->last_tot;

			uint64_t dt_us = ((uint64_t)dt_sec * 1000000ULL) >> 16;
			if (dt_us == 0)
				dt_us = 1;

			uint64_t raw_u64 = (delta * 100ULL << 16) / dt_us;
			r_some = (q16_t)raw_u64;
		}

		trend->cur = pg_kalman_update(&mon->filter, r_some, dt_sec);
		trend->vel = mon->filter.x_vel;
		trend->nis = mon->filter.nis;
	}
}

int pg_psi_read(struct pg_psi_monitor *RESTRICT mon,
		struct pg_psi_data *RESTRICT data,
		const struct timespec *RESTRICT now)
{
	if (mon->fd < 0) {
		LOGE("psi: invalid fd for read");
		return -EBADF;
	}

	ssize_t sz = pread(mon->fd, mon->buf, sizeof(mon->buf), 0);
	if (sz <= 0) {
		LOGE("psi: read failed or empty err=%d", errno);
		return -EIO;
	}

	q16_t dt_sec;
	if (mon->first_run)
		dt_sec = Q16_ONE;
	else {
		dt_sec = pg_dt_sec(&mon->last_read_ts, now);
		if (dt_sec < FLOAT_TO_Q16(0.001F))
			dt_sec = FLOAT_TO_Q16(0.001F);
	}

	struct pg_psi_trend trend = { 0 };
	uint64_t cur_tot = mon->last_tot;

	size_t pos = 0;
	size_t len = (size_t)sz;

	bool found = false;

	while (pos < len) {
		if (pos + 5 <= len &&
		    buffer_cmp(&mon->buf[pos], "some ", 5) == 0) {
			pos += 5;
			found = true;
			break;
		}

		while (pos < len && mon->buf[pos] != '\n')
			pos++;

		pos++;
	}

	if (found)
		parse(mon->buf, len, pos, mon->first_run, &trend, &cur_tot);

	calc_kalman(mon, &trend, cur_tot, dt_sec);

	mon->last_read_ts = *now;
	mon->last_tot = cur_tot;
	data->some = trend;

	return 0;
}

int pg_psi_read_raw(const char *RESTRICT path, q16_t *RESTRICT avg10)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	char buf[256];
	ssize_t sz = read(fd, buf, sizeof(buf));
	close(fd);

	if (sz <= 0)
		return -EIO;

	size_t pos = 0;
	size_t len = (size_t)sz;

	while (pos + 6 <= len) {
		if (buffer_cmp((const uint8_t *)&buf[pos], "avg10=", 6) == 0) {
			size_t n_pos;
			*avg10 = pg_parse_q16((const uint8_t *)buf, len,
					      pos + 6, &n_pos);
			return 0;
		}

		pos++;
	}

	return -ENOENT;
}
