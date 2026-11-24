// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_STATE_H
#define PGOV_STATE_H

#include "pgov/math.h"
#include "pgov/node.h"
#include "pgov/poll.h"
#include "pgov/psi.h"
#include "pgov/sched.h"
#include "pgov/sensor.h"
#include "pgov/thermal.h"
#include <stdbool.h>
#include <time.h>

struct context;
typedef void (*event_callback)(struct context *ctx);

enum display_state {
	DISP_UNKNOWN = 0,
	DISP_ON,
	DISP_GRACE,
	DISP_SUSPEND,
};

/*
 * Central state container, aligned to 64 bytes (cache line).
 * Mutated exclusively by the single reactor thread
 */
struct ALIGNED(64) context {
	struct psi_state psi_state;
	struct sched_state sched_state;
	struct poll_state poll_state;
	struct thermal_state therm_state;
	struct sensor_temp cpu_temp;
	struct sensor_temp bat_temp;
	struct sensor_battery battery;
	struct sensor_backlight backlight;
	struct node_cache sched_lat;
	struct node_cache sched_gran;
	struct node_cache sched_wake;
	struct node_cache sched_mig;
	struct node_cache sched_walt;
	struct node_cache sched_uclmin;
	struct node_cache sched_uclmax;

	enum display_state display_state;

	q16_t level_bat;
	q16_t temp_bat;
	q16_t therm_scale;

	struct timespec last_bat;
	struct timespec last_therm;
	struct timespec last_tick;
	struct timespec last_off;
	struct timespec last_disp;

#ifdef NDK_BUILD
	struct timespec last_sweep;
#endif // NDK_BUILD

	int efd;
	int sfd;
	int tfd;
	int lfd;
	int next_wake;

	uint8_t psi_fails;
	uint8_t temp_fails;

	bool remap_pending;
	bool dormant;
	volatile bool shutdown_req;

	event_callback on_trigger;
	event_callback on_timeout;
};

#endif // PGOV_STATE_H
