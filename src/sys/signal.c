// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "daemon/signal.h"
#include "daemon/logger.h"
#include "pascal_gov/compiler.h"
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

static void pascal_gov_crash_handler(int sig, siginfo_t *info, void *context)
{
	PASCAL_GOV_UNUSED(info);
	PASCAL_GOV_UNUSED(context);

	const char msg[] = "\n[PASCAL-GOV] fatal: daemon crashed\n";
	ssize_t unused = write(STDERR_FILENO, msg, sizeof(msg) - 1);
	PASCAL_GOV_UNUSED(unused);

	(void)signal(sig, SIG_DFL);
	(void)raise(sig);
}

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

void pascal_gov_signal_catch_crashes(void)
{
	struct sigaction sa = {0};
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = pascal_gov_crash_handler;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
}

void pascal_gov_signal_destroy(int fd)
{
	if (fd >= 0)
		close(fd);
}
