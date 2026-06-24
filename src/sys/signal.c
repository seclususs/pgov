// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "daemon/signal.h"
#include "daemon/logger.h"
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

int pascal_gov_signal_init(void)
{
	sigset_t mask;
	sigemptyset(&mask);

	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		int err = errno;
		LOGE("signal: failed to block default handlers err=%d", err);
		return -err;
	}

	int sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (sfd == -1) {
		int err = errno;
		LOGE("signal: failed to create signalfd err=%d", err);
		return -err;
	}

	return sfd;
}

void pascal_gov_signal_destroy(int fd)
{
	if (fd >= 0) {
		close(fd);
	}
}
