/*****************************************************************************
 * gpiomonitor.h
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
#ifndef __GPIOMONITOR_H__
#define __GPIOMONITOR_H__

#include <gpiod.h>

#ifndef MAX_GPIOS
#define MAX_GPIOS 64
#endif

#define GPIOD_LINE_OPTION_INPUT 0x00
#define GPIOD_LINE_OPTION_DEFAULT 0x01
#define GPIOD_LINE_OPTION_OUTPUT 0x02
#define GPIOD_LINE_OPTION_PULL_UP 0x04
#define GPIOD_LINE_OPTION_PULL_DOWN 0x08

typedef void (*handler_t)(void *ctx, int chipid, int line, struct gpiod_line_event *event);
typedef void (*free_ctx_t)(void *ctx);

int gpiod_addchip(struct gpiod_chip *handle);
const char *gpiod_chipname(int chipid);

int gpiod_addhandler(int gpioid, int action, void *ctx, handler_t callback, free_ctx_t fcallbak);
int gpiod_setline(int chipid, struct gpiod_line *handle, const char *name, int options);
const char *gpiod_name(int gpioid);
int gpiod_line(int gpioid);
int gpiod_state(int gpioid);
int gpiod_chipid(int gpioid);
int gpiod_eventable(int gpioid);
int gpiod_monitor();
void gpiod_stop();
void gpiod_free();

#endif
