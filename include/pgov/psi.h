// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PSI_H
#define PGOV_PSI_H

#include "pgov/kalman.h"
#include "pgov/math.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PSI_CPU "/proc/pressure/cpu"
#define PSI_THRESHOLD 100000
#define PSI_WINDOW 1000000

struct psi_metrics {
	q16_t stall_rate;
	q16_t velocity;
	q16_t avg10;
	q16_t avg300;
	q16_t nis;
};

struct psi_sample {
	struct psi_metrics some;
};

struct psi_state {
	int fd;
	uint8_t buf[512];
	struct timespec last_read_time;
	uint64_t last_total;
	bool initial;
	struct kalman_state filter;
};

bool psi_available(void);

/*
 * Open a pressure stall trigger file descriptor for epoll monitoring.
 * Caller owns the returned file descriptor and must close it
 *
 * Returns fd on success, or -errno on failure
 */
int psi_open(const char *path, int32_t threshold_us, int32_t window_us);

void psi_close(int fd);

int psi_recover(struct psi_state *state, const char *path);

void psi_init(struct psi_state *state, const char *path);

void psi_cleanup(struct psi_state *state);

/*
 * Read and parse pressure stall metrics without dynamic allocation,
 * then update the internal Kalman filter
 *
 * Returns 0 on success, or -errno on failure
 */
int psi_read_sample(struct psi_state *state, struct psi_sample *sample,
		    const struct timespec *now);

/* Read avg10 metric from pressure path. Returns 0 on success, or -errno */
int psi_read_avg10(const char *path, q16_t *avg10);

#endif // PGOV_PSI_H
