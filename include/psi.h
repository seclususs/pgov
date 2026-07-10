// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PSI_H
#define PGOV_PSI_H

#include "compiler.h"
#include "pg/kalman.h"
#include "pg/math.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct pg_psi_trend {
	q16_t cur;
	q16_t vel;
	q16_t avg10;
	q16_t avg300;
	q16_t nis;
};

struct pg_psi_data {
	struct pg_psi_trend some;
};

struct pg_psi_monitor {
	int fd;
	uint8_t buf[512];
	struct timespec last_read_ts;
	uint64_t last_tot;
	bool first_run;
	struct pg_kalman_state filter;
};

int pg_psi_open_trg(const char *path, int32_t threshold_us, int32_t window_us);

void pg_psi_close_trg(int fd);

int pg_psi_recov(struct pg_psi_monitor *RESTRICT mon,
		 const char *RESTRICT path);

void pg_psi_init(struct pg_psi_monitor *RESTRICT mon,
		 const char *RESTRICT path);

void pg_psi_cleanup(struct pg_psi_monitor *mon);

int pg_psi_read(struct pg_psi_monitor *RESTRICT mon,
		struct pg_psi_data *RESTRICT data,
		const struct timespec *RESTRICT now);

int pg_psi_read_raw(const char *RESTRICT path, q16_t *RESTRICT avg10);

#endif // PGOV_PSI_H
