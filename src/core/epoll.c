// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/epoll.h"
#include "pgov/log.h"
#include "pgov/state.h"
#include "pgov/compiler.h"
#include "pgov/time.h"
#include <errno.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENTS 16

static inline void epoll_error(struct context *ctx,
			       const struct timespec *wait_start)
{
	if (errno == EINTR) {
		if (ctx->next_wake < 0)
			return;

		struct timespec wait_end;
		clock_gettime(CLOCK_MONOTONIC, &wait_end);

		q16_t dt = time_dt_sec(wait_start, &wait_end);
		int elapsed = (int)((((q32_t)dt) * 1000) >> Q16_SHIFT);

		ctx->next_wake -= elapsed;
		if (ctx->next_wake < 1)
			ctx->next_wake = 1;
	} else {
		LOGE("epoll: wait err=%d, requesting shutdown", errno);
		ctx->shutdown_req = true;
	}
}

static inline void handle_signal_event(struct context *ctx)
{
	struct signalfd_siginfo s_info;
	ssize_t s;

	do {
		s = read(ctx->sfd, &s_info, sizeof(s_info));
	} while (s < 0 && errno == EINTR);

	if (s == sizeof(s_info))
		ctx->shutdown_req = true;
}

static inline void handle_trigger_event(struct context *ctx)
{
	if (LIKELY(ctx->on_trigger != NULL && !ctx->shutdown_req))
		ctx->on_trigger(ctx);
}

static inline void epoll_events(struct context *ctx, struct epoll_event *events,
				int nfds)
{
	for (int i = 0; i < nfds; ++i) {
		int ev_fd = events[i].data.fd;

		if (ev_fd == ctx->sfd)
			handle_signal_event(ctx);

		else if (ev_fd == ctx->tfd)
			handle_trigger_event(ctx);
	}
}

int pg_epoll_add_trg(struct context *ctx)
{
	if (UNLIKELY(!ctx))
		return -EINVAL;

	if (ctx->tfd < 0 || ctx->efd < 0)
		return -EBADF;

	struct epoll_event ev_trg = { 0 };
	ev_trg.events = EPOLLPRI | EPOLLERR | EPOLLET;
	ev_trg.data.fd = ctx->tfd;

	if (epoll_ctl(ctx->efd, EPOLL_CTL_ADD, ctx->tfd, &ev_trg) < 0)
		return -errno;

	return 0;
}

void pg_epoll_rm_trg(struct context *ctx)
{
	if (UNLIKELY(!ctx))
		return;

	if (ctx->tfd >= 0 && ctx->efd >= 0)
		epoll_ctl(ctx->efd, EPOLL_CTL_DEL, ctx->tfd, NULL);
}

static int epoll_setup(struct context *ctx)
{
	ctx->efd = epoll_create1(EPOLL_CLOEXEC);
	if (ctx->efd < 0)
		return -errno;

	struct epoll_event ev_sig = { 0 };
	ev_sig.events = EPOLLIN | EPOLLERR;
	ev_sig.data.fd = ctx->sfd;
	if (epoll_ctl(ctx->efd, EPOLL_CTL_ADD, ctx->sfd, &ev_sig) < 0) {
		int ret = -errno;
		LOGE("epoll: failed to register sfd err=%d", -ret);
		close(ctx->efd);
		ctx->efd = -1;
		return ret;
	}

	if (ctx->tfd >= 0) {
		int ret = pg_epoll_add_trg(ctx);
		if (ret < 0) {
			LOGE("epoll: failed to register tfd err=%d", -ret);
			close(ctx->efd);
			ctx->efd = -1;
			return ret;
		}
	}

	return 0;
}

int pg_epoll_run(struct context *ctx)
{
	if (UNLIKELY(!ctx))
		return -EINVAL;

	int ret = epoll_setup(ctx);
	if (ret < 0)
		return ret;

	struct epoll_event events[MAX_EVENTS];

	while (LIKELY(!ctx->shutdown_req)) {
		struct timespec wait_start;
		clock_gettime(CLOCK_MONOTONIC, &wait_start);

		int nfds = epoll_wait(ctx->efd, events, MAX_EVENTS,
				      ctx->next_wake);

		if (UNLIKELY(nfds < 0)) {
			epoll_error(ctx, &wait_start);
			continue;
		}

		if (nfds == 0) {
			if (LIKELY(ctx->on_timeout != NULL))
				ctx->on_timeout(ctx);

			continue;
		}

		epoll_events(ctx, events, nfds);
	}

	close(ctx->efd);
	ctx->efd = -1;
	return 0;
}
