// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/signal.h"
#include "pgov/log.h"
#include "pgov/compiler.h"
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

#define NR_CRASH_SIGS 4

static const int CRASH_SIGS[NR_CRASH_SIGS] = {
	SIGSEGV,
	SIGFPE,
	SIGABRT,
	SIGILL,
};

static struct sigaction prevsa[NR_CRASH_SIGS];
static void (*crash_hook)(void);

static void crash_handler(int sig, siginfo_t *info, void *sigctx)
{
	const char msg[] = "\n[PGOV] fatal: daemon crashed\n";
	const struct sigaction *old;
	void (*hook)(void);
	ssize_t res;
	int i;

	res = write(STDERR_FILENO, msg, sizeof(msg) - 1);
	UNUSED(res);

	hook = crash_hook;
	crash_hook = NULL;
	if (hook)
		hook();

	for (i = 0; i < NR_CRASH_SIGS; i++) {
		if (CRASH_SIGS[i] != sig)
			continue;

		old = &prevsa[i];
		if (old->sa_flags & SA_SIGINFO) {
			if (old->sa_sigaction)
				old->sa_sigaction(sig, info, sigctx);

		} else if (old->sa_handler != SIG_DFL &&
			   old->sa_handler != SIG_IGN) {
			old->sa_handler(sig);
		}

		break;
	}

	(void)signal(sig, SIG_DFL);
	(void)raise(sig);
}

int signal_init(void)
{
	sigset_t mask;
	int err;
	int sfd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		err = errno;
		LOGE("signal: failed to block default handlers err=%d", err);
		return -err;
	}

	sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (sfd == -1) {
		err = errno;
		LOGE("signal: failed to create signalfd err=%d", err);
		goto out_unblock;
	}

	return sfd;

out_unblock:
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	return -err;
}

void signal_catch_crash(void (*hook)(void))
{
	struct sigaction sa = { 0 };
	int i;

	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = crash_handler;
	sigemptyset(&sa.sa_mask);

	crash_hook = hook;

	for (i = 0; i < NR_CRASH_SIGS; i++)
		sigaction(CRASH_SIGS[i], &sa, &prevsa[i]);
}

void signal_close(int fd)
{
	if (fd >= 0)
		close(fd);
}
