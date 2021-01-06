/*****************************************************************************
 * export.c
 * this file is part of https://github.com/mchalain/gpiod
 *****************************************************************************
 * Copyright (C) 2016-2020
 *
 * Authors: Marc Chalain <marc.chalain@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/wait.h>
#include <pthread.h>

#include "log.h"
#include "gpiomonitor.h"
#include "export.h"

typedef struct export_s export_t;
typedef struct export_event_s export_event_t;

struct export_event_s
{
	struct gpiod_line_event event;
	int chipid;
	int gpioid;
	struct export_event_s *next;
};

typedef int (*export_accept_t)(export_t *ctx);
typedef int (*export_format_t)(const export_t *ctx, int socket, export_event_t *event);
struct export_s
{
	int socket;
	export_accept_t accept;
	export_format_t format;
	int rootfd;
	char *url;
	int run;
	export_event_t default_event;
	export_event_t *events;
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

};

static int export_format_raw(const export_t *ctx, int socket, export_event_t *event);
static int export_format_json(const export_t *ctx, int socket, export_event_t *event);

static int export_accept_fifo(export_t *ctx);

static int export_status(export_t *ctx);
static void export_freeevents(export_t *ctx);

static const char str_rising[] = "rising";
static const char str_falling[] = "falling";
static const char str_unamed[] = "unnamed";

static void *export_thread(void * arg)
{
	export_t *ctx = (export_t *)arg;
	while (ctx->run)
	{
		int socket = ctx->accept(ctx);
	}
	return NULL;
}

static void *export_threadevent(void * arg)
{
	export_t *ctx = (export_t *)arg;
	while (ctx->run)
	{
		pthread_mutex_lock(&ctx->mutex);
		while (ctx->events == NULL)
			pthread_cond_wait(&ctx->cond, &ctx->mutex);
		while (ctx->events != NULL)
		{
			ctx->format(ctx, ctx->socket, ctx->events);
			ctx->events = ctx->events->next;
		}
		pthread_mutex_unlock(&ctx->mutex);
	}
	return NULL;
}

void *export_create(int rootfd, const char *url, const char *format, int gpioid)
{
	export_t *ctx = calloc(1, sizeof(*ctx));
	ctx->rootfd = rootfd;
	ctx->default_event.gpioid = gpioid;
	ctx->default_event.chipid = gpiod_chipid(gpioid);
	dbg("export: export %s", url);
	if (!strncmp(url, "fifo://", 7))
	{
		unlinkat(rootfd, url + 7, 0);
		if (mkfifoat(rootfd, url + 7, 0666) == -1)
			err("gpiod: export %s %s", url + 7, strerror(errno));
		ctx->url = strdup(url);
		ctx->accept = &export_accept_fifo;
	}
	if (!strcmp(format, "raw"))
	{
		ctx->format = &export_format_raw;
	}
	if (!strcmp(format, "json"))
	{
		ctx->format = &export_format_json;
	}
	ctx->run = 1;
	pthread_cond_init(&ctx->cond, NULL);
	pthread_mutex_init(&ctx->mutex, NULL);
	pthread_create(&ctx->thread, NULL, export_thread, ctx);
	pthread_detach(ctx->thread);
	return ctx;
}

static int export_format_raw(const export_t *ctx, int socket, export_event_t *event)
{
	return write(socket, &event->event, sizeof(event->event));
}

static int export_format_json(const export_t *ctx, int socket, export_event_t *event)
{
	const char *name = gpiod_name(event->gpioid);
	if (name == NULL)
		name = str_unamed;
	int ret;
	ret = dprintf(socket,
			"{\"chip\" = %.2d; " \
			"\"gpio\" = %.2d; " \
			"\"name\" = \"%s\"; " \
			"\"status\" = \"%s\";}\n",
			event->chipid, gpiod_line(event->gpioid), name,
			(event->event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)? str_rising: str_falling);
	return ret;
}

static int export_status(export_t *ctx)
{
	int value = gpiod_state(ctx->default_event.gpioid);
	if (value == -1)
		return -1;
	if (value == 1)
		ctx->default_event.event.event_type = GPIOD_LINE_EVENT_RISING_EDGE;
	else if (value == 0)
		ctx->default_event.event.event_type = GPIOD_LINE_EVENT_FALLING_EDGE;
	export_run(ctx, ctx->default_event.chipid, ctx->default_event.gpioid, &ctx->default_event.event);
	return 0;
}

static int export_accept_fifo(export_t *ctx)
{
	/// open block while a reader doesn't open the fifo
	ctx->socket = openat(ctx->rootfd, ctx->url + 7, O_WRONLY);
	dbg("export: new fifo client on %s", ctx->url + 7);
	/// only one reader is possible with a fifo
	if (ctx->socket != -1)
	{
		export_freeevents(ctx);
		/// create an event with the current status of the line
		export_status(ctx);
		if (gpiod_eventable(ctx->default_event.gpioid))
		{
			/// start the loop to trigg the events
			export_threadevent(ctx);
		}
		else
		{
			/// send only the current status and die
			pthread_mutex_lock(&ctx->mutex);
			while (ctx->events != NULL)
			{
				ctx->format(ctx, ctx->socket, ctx->events);
				ctx->events = ctx->events->next;
			}
			pthread_mutex_unlock(&ctx->mutex);
			close(ctx->socket);
			ctx->socket = -1;
			/// cat try to read several time if closing is too fast
			usleep(200000);
		}
	}
	else
		err("gpiod: open %s %s", ctx->url + 7, strerror(errno));
	return ctx->socket;
}

void export_run(void *arg, int chipid, int gpioid, struct gpiod_line_event *event)
{
	export_t *ctx = (export_t *)arg;

	pthread_mutex_lock(&ctx->mutex);
	export_event_t *new = calloc(1, sizeof(*new));
	new->chipid = chipid;
	new->gpioid = gpioid;
	memcpy(&new->event, event, sizeof(new->event));
	export_event_t *old = ctx->events;
	if (old == NULL)
		ctx->events = new;
	else
	{
		while (old->next != NULL) old = old->next;
		old->next = new;
	}
	dbg("export: event");
	pthread_mutex_unlock(&ctx->mutex);

	pthread_cond_broadcast(&ctx->cond);
	return;
}

static void export_freeevents(export_t *ctx)
{
	export_event_t *old = ctx->events;
	while (old != NULL)
	{
		ctx->events = old->next;
		free(old);
		old = ctx->events;
	}
}

void export_free(void *arg)
{
	export_t *ctx = (export_t *)arg;
	if (ctx->socket >= 0)
		close(ctx->socket);
	pthread_cond_destroy(&ctx->cond);
	pthread_mutex_destroy(&ctx->mutex);
	export_freeevents(ctx);
	free(ctx->url);
	free(ctx);
}
