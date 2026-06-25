// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_PSI_H
#define PASCAL_GOV_PSI_H

#include "pascal_gov/compiler.h"
#include "pascal_gov/filter.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
	float current;
	float velocity;
	float avg10;
	float avg300;
	float nis;
} pascal_gov_psi_trend;

typedef struct {
	pascal_gov_psi_trend some;
} pascal_gov_psi_data;

typedef struct {
	int fd;
	uint8_t buffer[512];
	struct timespec last_read_time;
	uint64_t last_some_total;
	bool first_run;
	pascal_gov_kalman_state filter_some;
} pascal_gov_psi_monitor;

int pascal_gov_psi_register_trigger(const char *path, int32_t threshold_us,
				    int32_t window_us);

void pascal_gov_psi_unregister_trigger(int fd);

int pascal_gov_psi_recover(pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor,
			   const char *path);

void pascal_gov_psi_init(
	pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor, const char *path,
	const pascal_gov_kalman_config *PASCAL_GOV_RESTRICT config);

void pascal_gov_psi_destroy(
	pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor);

int pascal_gov_psi_read_state(
	pascal_gov_psi_monitor *PASCAL_GOV_RESTRICT monitor,
	pascal_gov_psi_data *PASCAL_GOV_RESTRICT out_data,
	const struct timespec *PASCAL_GOV_RESTRICT now);

#endif // PASCAL_GOV_PSI_H
