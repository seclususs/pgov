// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#define _GNU_SOURCE
#include "pgov/epoll.h"
#include "pgov/compiler.h"
#include "pgov/log.h"
#include "pgov/state.h"
#include "pgov/time.h"
#include <errno.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENT 16

static inline void handle_error(struct context *ctx,
				const struct timespec *start)
{
	struct timespec now;
	q16_t dt;
	int ms;

	if (errno == EINTR) {
		if (ctx->next_wake < 0)
			return;

		clock_gettime(CLOCK_MONOTONIC, &now);

		dt = time_dt_sec(start, &now);
		ms = (int)((((q32_t)dt) * 1000) >> Q16_SHIFT);

		ctx->next_wake -= ms;
		if (ctx->next_wake < 1)
			ctx->next_wake = 1;
	} else {
		LOGE("epoll: wait err=%d, requesting shutdown", errno);
		ctx->shutdown_req = true;
	}
}

static inline void handle_signal(struct context *ctx)
{
	struct signalfd_siginfo info;
	ssize_t res;

	do {
		res = read(ctx->sfd, &info, sizeof(info));
	} while (res < 0 && errno == EINTR);

	if (res == sizeof(info))
		ctx->shutdown_req = true;
}

static inline void handle_trigger(struct context *ctx)
{
	if (LIKELY(ctx->on_trigger != NULL && !ctx->shutdown_req))
		ctx->on_trigger(ctx);
}

static inline void dispatch_events(struct context *ctx,
				   struct epoll_event *events, int ready)
{
	int fd;
	int i;

	for (i = 0; i < ready; ++i) {
		fd = events[i].data.fd;

		if (fd == ctx->sfd)
			handle_signal(ctx);
		else if (fd == ctx->tfd)
			handle_trigger(ctx);
	}
}

int epoll_add_trigger(struct context *ctx)
{
	struct epoll_event event = { 0 };

	if (UNLIKELY(!ctx))
		return -EINVAL;

	if (ctx->tfd < 0 || ctx->efd < 0)
		return -EBADF;

	event.events = EPOLLPRI | EPOLLERR | EPOLLET;
	event.data.fd = ctx->tfd;

	if (epoll_ctl(ctx->efd, EPOLL_CTL_ADD, ctx->tfd, &event) < 0)
		return -errno;

	return 0;
}

void epoll_rm_trigger(struct context *ctx)
{
	if (UNLIKELY(!ctx))
		return;

	if (ctx->tfd >= 0 && ctx->efd >= 0)
		epoll_ctl(ctx->efd, EPOLL_CTL_DEL, ctx->tfd, NULL);
}

static int setup(struct context *ctx)
{
	struct epoll_event event = { 0 };
	int ret;

	ctx->efd = epoll_create1(EPOLL_CLOEXEC);
	if (ctx->efd < 0) {
		ret = -errno;
		LOGE("epoll: failed to create efd err=%d", errno);
		return ret;
	}

	event.events = EPOLLIN | EPOLLERR;
	event.data.fd = ctx->sfd;
	if (epoll_ctl(ctx->efd, EPOLL_CTL_ADD, ctx->sfd, &event) < 0) {
		ret = -errno;
		LOGE("epoll: failed to register sfd err=%d", -ret);
		goto err_close_efd;
	}

	if (ctx->tfd >= 0) {
		ret = epoll_add_trigger(ctx);
		if (ret < 0) {
			LOGE("epoll: failed to register tfd err=%d", -ret);
			goto err_close_efd;
		}
	}

	return 0;

err_close_efd:
	close(ctx->efd);
	ctx->efd = -1;
	return ret;
}

int epoll_run(struct context *ctx)
{
	struct epoll_event events[MAX_EVENT];
	struct timespec start;
	int ready;
	int ret;

	if (UNLIKELY(!ctx))
		return -EINVAL;

	ret = setup(ctx);
	if (ret < 0)
		return ret;

	while (LIKELY(!ctx->shutdown_req)) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		ready = epoll_wait(ctx->efd, events, MAX_EVENT, ctx->next_wake);

		if (UNLIKELY(ready < 0)) {
			handle_error(ctx, &start);
			continue;
		}

		if (ready == 0) {
			if (LIKELY(ctx->on_timeout != NULL))
				ctx->on_timeout(ctx);

			continue;
		}

		dispatch_events(ctx, events, ready);
	}

	close(ctx->efd);
	ctx->efd = -1;
	return 0;
}
