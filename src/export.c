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

#include "log.h"
#include "gpiomonitor.h"

typedef struct export_s export_t;
typedef int (*export_format_t)(const export_t *ctx, int chipid, int gpioid, struct gpiod_line_event *event);
struct export_s
{
	int socket;
	export_format_t format;
	int rootfd;
	const char *url;
};

static int export_format_raw(const export_t *ctx, int chipid, int gpioid, struct gpiod_line_event *event);

static const char str_rising[] = "rising";
static const char str_falling[] = "falling";
static const char str_empty[] = "";
#define NUMENVS 3
static const char str_GPIOENV[] = "GPIO=%.2d";
static const char str_CHIPENV[] = "CHIP=%.2d";
static const char str_NAMEENV[] = "NAME=%s";
static const char str_ACTIONENV[] = "ACTION=%s     ";

void *export_create(int rootfd, const char *url, const char *format)
{
	export_t *ctx = calloc(1, sizeof(*ctx));
	ctx->rootfd = rootfd;
	if (!strncmp(url, "fifo://", 7))
	{
		unlinkat(rootfd, url + 7, 0);
		mkfifoat(rootfd, url + 7, 0666);
		ctx->socket = openat(rootfd, url + 7, O_RDWR);
		if (ctx->socket == -1)
		{
			err("export: create fifo error %s", strerror(errno));
			free(ctx);
			return NULL;
		}
		ctx->url = url;
	}
	if (!strcmp(format, "raw"))
	{
		ctx->format = &export_format_raw;
	}
	return ctx;
}

static int export_format_raw(const export_t *ctx, int chipid, int gpioid, struct gpiod_line_event *event)
{
	return write(ctx->socket, event, sizeof(*event));
}

void export_run(void *arg, int chipid, int gpioid, struct gpiod_line_event *event)
{
	const export_t *ctx = (const export_t *)arg;
	if (ctx->format != NULL)
		ctx->format(ctx, chipid, gpioid, event);
	return;
}

void export_free(void *arg)
{
	export_t *ctx = (export_t *)arg;
	close(ctx->socket);
	if (!strncmp(ctx->url, "fifo://", 7))
	{
		unlinkat(ctx->rootfd, ctx->url + 7, 0);
	}
	free(ctx);
}
