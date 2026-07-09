// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "epoll.h"
#include "pg/config.h"
#include "pg/log.h"
#include "pg/state.h"
#include "compiler.h"
#include "pg/time.h"
#include <errno.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

static inline void epoll_error(struct pg_context *RESTRICT ctx,
			       const struct timespec *RESTRICT wait_start)
{
	if (errno == EINTR) {
		struct timespec wait_end;
		clock_gettime(CLOCK_MONOTONIC, &wait_end);

		q16_t dt = pg_dt_sec(wait_start, &wait_end);
		int elapsed = (int)((((q32_t)dt) * 1000) >> Q16_SHIFT);

		ctx->next_wake -= elapsed;
		if (ctx->next_wake < 1)
			ctx->next_wake = 1;
	} else {
		LOGE("epoll: wait err=%d, requesting shutdown", errno);
		ctx->shutdown_req = true;
	}
}

static inline void epoll_events(struct pg_context *RESTRICT ctx,
				struct epoll_event *RESTRICT events, int nfds)
{
	for (int i = 0; i < nfds; ++i) {
		int ev_fd = events[i].data.fd;

		if (ev_fd == ctx->sig_fd) {
			struct signalfd_siginfo s_info;
			ssize_t s = read(ctx->sig_fd, &s_info, sizeof(s_info));

			if (s == sizeof(s_info))
				ctx->shutdown_req = true;

		} else if (ev_fd == ctx->trg_fd) {
			if (LIKELY(ctx->on_trigger != NULL))
				ctx->on_trigger(ctx);
		}
	}
}

int pg_epoll_add_trg(struct pg_context *ctx)
{
	if (ctx->trg_fd < 0 || ctx->epoll_fd < 0)
		return -EBADF;

	struct epoll_event ev_trg = { 0 };
	ev_trg.events = EPOLLPRI | EPOLLERR | EPOLLET;
	ev_trg.data.fd = ctx->trg_fd;

	if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->trg_fd, &ev_trg) < 0)
		return -errno;

	return 0;
}

void pg_epoll_rm_trg(struct pg_context *ctx)
{
	if (ctx->trg_fd >= 0 && ctx->epoll_fd >= 0)
		epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, ctx->trg_fd, NULL);
}

int pg_epoll_run(struct pg_context *ctx)
{
	ctx->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (ctx->epoll_fd < 0)
		return -errno;

	struct epoll_event ev_sig = { 0 };
	ev_sig.events = EPOLLIN | EPOLLERR;
	ev_sig.data.fd = ctx->sig_fd;
	epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->sig_fd, &ev_sig);

	struct epoll_event ev_trg = { 0 };
	ev_trg.events = EPOLLPRI | EPOLLERR | EPOLLET;
	ev_trg.data.fd = ctx->trg_fd;
	epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->trg_fd, &ev_trg);

	struct epoll_event events[PG_MAX_EVENTS];

	while (LIKELY(!ctx->shutdown_req)) {
		struct timespec wait_start;
		clock_gettime(CLOCK_MONOTONIC, &wait_start);

		int nfds = epoll_wait(ctx->epoll_fd, events, PG_MAX_EVENTS,
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

	close(ctx->epoll_fd);
	ctx->epoll_fd = -1;
	return 0;
}
