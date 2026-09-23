// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_STATE_H
#define PG_STATE_H

#include "pg/cpu.h"
#include "pg/poll.h"
#include "psi.h"
#include "sensor.h"
#include "sysfs.h"
#include "pg/thermal.h"
#include "pg/math.h"
#include <stdbool.h>
#include <time.h>

struct pg_context;
typedef void (*pg_event_cb)(struct pg_context *ctx);

enum pg_disp_state {
	PG_DISP_UNKNOWN = 0,
	PG_DISP_ON,
	PG_DISP_GRACE,
	PG_DISP_SUSPEND
};

/**
 * @brief The centralized, cache-aligned state machine container for the daemon.
 *
 * @note MEMORY LAYOUT & ARCHITECTURE:
 *       - Declared with `ALIGNED(64)` to strictly fit into standard CPU L1 cache lines.
 *         Any addition or rearrangement of struct members MUST be carefully audited to 
 *         prevent cache line bouncing or unnecessary padding inflation.
 *       - Encapsulates all hardware sensors, sysfs file descriptors, and mathematical 
 *         integrators (PID/Kalman). 
 *       - The lifecycle of this object spans the entire daemon execution; it must remain 
 *         valid and strictly mutated by a single event-reactor thread.
 */
struct ALIGNED(64) pg_context {
	struct pg_psi_monitor psi;
	struct pg_temp_sensor cpu_temp_sensor;
	struct pg_temp_sensor bat_temp_sensor;
	struct pg_bat_sensor bat_cap_sensor;
	struct pg_bl_sensor bl_sensor;
	struct pg_sysfs_cache sched_lat;
	struct pg_sysfs_cache sched_gran;
	struct pg_sysfs_cache sched_wake;
	struct pg_sysfs_cache sched_mig;
	struct pg_sysfs_cache sched_walt;
	struct pg_sysfs_cache sched_ucl;
	struct pg_thermal_state thermal_state;
	struct pg_load_state load_state;
	struct pg_poll_state poll_state;

	enum pg_disp_state disp_state;

	q16_t bat_lvl;
	q16_t bat_temp;
	q16_t cached_th_scl;

	struct timespec last_bat;
	struct timespec last_therm;
	struct timespec last_tick;
	struct timespec last_dispoff;

#if defined(NDK_BUILD)
	struct timespec last_sweep;
#endif // NDK_BUILD

	int epoll_fd;
	int sig_fd;
	int trg_fd;
	int lock_fd;
	int next_wake;

	volatile bool shutdown_req;

	pg_event_cb on_trigger;
	pg_event_cb on_timeout;
};

#endif // PG_STATE_H
