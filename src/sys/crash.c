// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "daemon/crash.h"
#include <signal.h>
#include <unistd.h>

static void signal_handler(int sig, siginfo_t *info, void *context)
{
	(void)info;
	(void)context;

	const char msg[] = "\n[PASCAL-GOV] fatal: daemon crashed\n";
	ssize_t unused = write(STDERR_FILENO, msg, sizeof(msg) - 1);
	(void)unused;

	(void)signal(sig, SIG_DFL);
	(void)raise(sig);
}

void pascal_gov_crash_init(void)
{
	struct sigaction sa = {0};
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = signal_handler;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
}
