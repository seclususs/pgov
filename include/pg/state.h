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

struct ALIGNED(64) pg_context {
	struct pg_psi_monitor psi;
	struct pg_temp_sensor cpu_temp_sensor;
	struct pg_temp_sensor bat_temp_sensor;
	struct pg_bat_sensor bat_cap_sensor;
	struct pg_sysfs_cache sched_lat;
	struct pg_sysfs_cache sched_gran;
	struct pg_sysfs_cache sched_wake;
	struct pg_sysfs_cache sched_mig;
	struct pg_sysfs_cache sched_walt;
	struct pg_sysfs_cache sched_ucl;
	struct pg_thermal_state thermal_state;
	struct pg_load_state load_state;
	struct pg_poll_state poll_state;
	struct pg_bl_sensor bl_sensor;

	q16_t bat_lvl;
	q16_t bat_temp;

	struct timespec last_bat;
	struct timespec last_tick;

#if defined(NDK_BUILD)
	struct timespec last_sweep;
#endif // NDK_BUILD

	int epoll_fd;
	int sig_fd;
	int trg_fd;
	int next_wake;

	volatile bool shutdown_req;

	pg_event_cb on_trigger;
	pg_event_cb on_timeout;
};

#endif // PG_STATE_H
