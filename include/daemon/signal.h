// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_DAEMON_SIGNAL_H
#define PASCAL_GOV_DAEMON_SIGNAL_H

int pascal_gov_signal_init(void);
void pascal_gov_signal_crash(void);
void pascal_gov_signal_destroy(int fd);

#endif // PASCAL_GOV_DAEMON_SIGNAL_H
