// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_PSI_H
#define PGOV_PSI_H

#include "pgov/kalman.h"
#include "pgov/math.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PSI_CPU_PATH "/proc/pressure/cpu"
#define PSI_THRESHOLD_US 100000
#define PSI_WINDOW_US 1000000

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
	uint8_t buffer[512];
	struct timespec last_read_time;
	uint64_t last_total;
	bool initial;
	struct pg_kalman_state filter;
};

bool psi_available(void);

int psi_trigger_open(const char *path, int32_t threshold_us, int32_t window_us);

void psi_trigger_close(int fd);

int psi_recover(struct psi_state *state, const char *path);

void psi_init(struct psi_state *state, const char *path);

void psi_cleanup(struct psi_state *state);

/**
 * @brief Parses raw Linux PSI strings and feeds the data into the Kalman filter.
 *
 * @param state  Pointer to the initialized PSI state machine. Borrowed for mutation.
 * @param sample Output structure to store the computed PSI metrics.
 * @param now    Current monotonic timestamp for dt (delta time) calculations.
 *
 * @note IMPLICIT CONTRACTS & SYSTEM IMPACT:
 *       - Performs zero-allocation string parsing directly on the state's fixed 512-byte
 *         buffer (`state->buffer`) to minimize hot-path latency.
 *       - Implements a resilient `pread` loop to bypass seeking overhead.
 *       - Deeply mutates `state->filter` (Kalman state) by injecting the newly parsed metrics.
 *
 * @return 0 on successful parse and filter update, or a negative POSIX error code.
 */
int psi_read(struct psi_state *state, struct psi_sample *sample,
	     const struct timespec *now);

int psi_read_avg10(const char *path, q16_t *avg10);

#endif // PGOV_PSI_H
