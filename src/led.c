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

static const char str_rising[] = "rising";
static const char str_falling[] = "falling";
static const char str_empty[] = "";
#define NUMENVS 3
static const char str_GPIOENV[] = "GPIO=%.2d";
static const char str_CHIPENV[] = "CHIP=%.2d";
static const char str_NAMEENV[] = "NAME=%s";

void *led_create(struct gpiod_line *handle, int bare)
{
	led_t *ctx = calloc(1, sizeof(*ctx));
	ctx->handle = handle;
	ctx->bare = bare;
	gpiod_line_request_output(handle, "gpiod", 0);
	
	return ctx;
}

void led_run(void *arg, int chipid, int line, struct gpiod_line_event *event)
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
