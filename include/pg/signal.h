// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PG_SIGNAL_H
#define PG_SIGNAL_H

int pg_signal_init(void);
void pg_signal_catch_crash(void);
void pg_signal_close(int fd);

#endif // PG_SIGNAL_H
