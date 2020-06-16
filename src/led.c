/*****************************************************************************
 * led.c
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

#include "log.h"
#include "gpiomonitor.h"

typedef struct led_s led_t;
struct led_s
{
	struct gpiod_line *handle;
	int bare;
};

void *led_create(struct gpiod_line *handle, int bare)
{
	led_t *ctx = calloc(1, sizeof(*ctx));
	ctx->handle = handle;
	ctx->bare = bare;
	gpiod_line_request_output(handle, "gpiod", 0);

	return ctx;
}

void led_run(void *arg, int chipid, int gpioid, struct gpiod_line_event *event)
{
	led_t *ctx = (led_t *)arg;
	unsigned int val;
	if (event->event_type == GPIOD_LINE_EVENT_RISING_EDGE)
		val = 1 ^ ctx->bare;
	else
		val = 0 ^ ctx->bare;
	gpiod_line_set_value(ctx->handle, val);

	return;
}

void led_free(void *arg)
{
	led_t *ctx = (led_t *)arg;
	gpiod_line_release(ctx->handle);
	free(ctx);
}
