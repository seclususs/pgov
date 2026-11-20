// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/psi.h"
#include "pgov/log.h"
#include "pgov/parser.h"
#include "pgov/time.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

bool psi_available(void)
{
	if (access(PSI_CPU_PATH, R_OK | W_OK) == 0)
		return true;

	LOGE("psi: cpu node missing or access denied");
	return false;
}

int psi_trigger_open(const char *path, int32_t threshold_us, int32_t window_us)
{
	ssize_t res;
	char payload[64];
	size_t pos;
	int fd;
	int err;

	if (UNLIKELY(!path))
		return -EINVAL;

	fd = open(path, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0) {
		err = errno;
		LOGE("psi: failed to open trigger node %s err=%d", path, err);
		return -err;
	}

	payload[0] = 's';
	payload[1] = 'o';
	payload[2] = 'm';
	payload[3] = 'e';
	payload[4] = ' ';
	pos = 5;

	pos += pg_fmt_u32(threshold_us, payload + pos);
	payload[pos++] = ' ';

	pos += pg_fmt_u32(window_us, payload + pos);
	payload[pos++] = '\n';

	do {
		res = write(fd, payload, pos);
	} while (res < 0 && errno == EINTR);

	if (res < 0) {
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

void psi_trigger_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

int psi_recover(struct psi_state *state, const char *path)
{
	if (UNLIKELY(!state || !path))
		return -EINVAL;

	if (state->fd >= 0) {
		close(state->fd);
		state->fd = -1;
	}

	state->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (state->fd < 0)
		return -errno;

	pg_kalman_reset(&state->filter);
	state->initial = true;
	return 0;
}

void psi_init(struct psi_state *state, const char *path)
{
	if (UNLIKELY(!state || !path))
		return;

	state->fd = open(path, O_RDONLY | O_CLOEXEC);
	if (state->fd < 0)
		LOGE("psi: failed to open node %s", path);

	clock_gettime(CLOCK_MONOTONIC, &state->last_read_time);
	state->last_total = 0;
	state->initial = true;
	pg_kalman_init(&state->filter);
}

void psi_cleanup(struct psi_state *state)
{
	if (UNLIKELY(!state))
		return;

	if (state->fd >= 0) {
		close(state->fd);
		state->fd = -1;
	}
}

static inline bool match_token(const uint8_t *buf, const char *str, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (buf[i] != (uint8_t)str[i])
			return false;
	}

	return true;
}

static inline size_t skip_token(const uint8_t *buf, size_t len, size_t pos)
{
	while (pos < len && buf[pos] != ' ' && buf[pos] != '\n')
		pos++;

	return pos;
}

static inline size_t parse_metrics(const uint8_t *RESTRICT buf, size_t len,
				   size_t pos, bool initial,
				   struct psi_metrics *RESTRICT metrics,
				   uint64_t *RESTRICT cur_total)
{
	uint64_t parsed_total;
	size_t next_pos;

	while (pos < len && buf[pos] != '\n') {
		if (buf[pos] == ' ') {
			pos++;
			continue;
		}

		if (pos + 6 <= len && match_token(&buf[pos], "avg10=", 6)) {
			if (LIKELY(!initial)) {
				pos = skip_token(buf, len, pos + 6);
				continue;
			}

			metrics->avg10 =
				pg_parse_q16(buf, len, pos + 6, &next_pos);

			pos = next_pos;
			continue;
		}

		if (pos + 7 <= len && match_token(&buf[pos], "avg300=", 7)) {
			metrics->avg300 =
				pg_parse_q16(buf, len, pos + 7, &next_pos);

			pos = next_pos;
			continue;
		}

		if (pos + 6 <= len && match_token(&buf[pos], "total=", 6)) {
			parsed_total =
				pg_parse_u64(buf, len, pos + 6, &next_pos);

			if (LIKELY(next_pos > pos + 6))
				*cur_total = parsed_total;

			pos = next_pos;
			continue;
		}

		pos = skip_token(buf, len, pos);
	}

	return pos;
}

static inline void update_filter(struct psi_state *state,
				 struct psi_metrics *metrics,
				 uint64_t cur_total, q16_t dt_sec)
{
	uint64_t instant_rate;
	uint64_t delta;
	uint64_t dt_us;
	q16_t stall_rate;

	if (state->initial) {
		metrics->stall_rate = metrics->avg10;
		metrics->velocity = 0;

		pg_kalman_reset(&state->filter);
		pg_kalman_update(&state->filter, metrics->avg10, Q16_ONE);
		state->initial = false;
		return;
	}

	stall_rate = 0;
	if (LIKELY(cur_total > state->last_total)) {
		delta = cur_total - state->last_total;

		dt_us = ((uint64_t)dt_sec * 1000000ULL) >> 16;
		if (dt_us == 0)
			dt_us = 1;

		instant_rate = (delta * 100ULL << 16) / dt_us;
		if (instant_rate > (uint64_t)INT_TO_Q16(500))
			stall_rate = INT_TO_Q16(500);
		else
			stall_rate = (q16_t)instant_rate;
	}

	metrics->stall_rate =
		pg_kalman_update(&state->filter, stall_rate, dt_sec);

	metrics->velocity = state->filter.x_vel;
	metrics->nis = state->filter.nis;
}

int psi_read(struct psi_state *state, struct psi_sample *sample,
	     const struct timespec *now)
{
	struct psi_metrics metrics = { 0 };
	ssize_t res;
	uint64_t cur_total;
	q16_t dt_sec;
	size_t len;
	size_t pos = 0;
	bool found = false;

	if (UNLIKELY(!state || !sample || !now))
		return -EINVAL;

	if (state->fd < 0) {
		LOGE("psi: invalid fd for read");
		return -EBADF;
	}

	do {
		res = pread(state->fd, state->buf, sizeof(state->buf), 0);
	} while (res < 0 && errno == EINTR);

	if (res <= 0) {
		LOGE("psi: read failed or empty err=%d", errno);
		return -EIO;
	}

	if (state->initial) {
		dt_sec = Q16_ONE;
	} else {
		dt_sec = pg_dt_sec(&state->last_read_time, now);
		if (dt_sec < FLOAT_TO_Q16(0.001F))
			dt_sec = FLOAT_TO_Q16(0.001F);
	}

	cur_total = state->last_total;
	len = (size_t)res;

	while (pos < len) {
		if (pos + 5 <= len &&
		    match_token(&state->buf[pos], "some ", 5)) {
			pos += 5;
			found = true;
			break;
		}

		while (pos < len && state->buf[pos] != '\n')
			pos++;

		pos++;
	}

	if (!found)
		return -ENODATA;

	parse_metrics(state->buf, len, pos, state->initial, &metrics,
		      &cur_total);

	update_filter(state, &metrics, cur_total, dt_sec);

	state->last_read_time = *now;
	state->last_total = cur_total;
	sample->some = metrics;

	return 0;
}

int psi_read_avg10(const char *path, q16_t *avg10)
{
	uint8_t buf[256];
	ssize_t res;
	size_t next_pos;
	size_t len;
	size_t pos = 0;
	int fd;

	if (UNLIKELY(!path || !avg10))
		return -EINVAL;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	do {
		res = read(fd, buf, sizeof(buf));
	} while (res < 0 && errno == EINTR);

	close(fd);

	if (res <= 0)
		return -EIO;

	len = (size_t)res;
	while (pos + 6 <= len) {
		if (match_token(&buf[pos], "avg10=", 6)) {
			*avg10 = pg_parse_q16(buf, len, pos + 6, &next_pos);
			return 0;
		}

		pos++;
	}

	return -ENOENT;
}
