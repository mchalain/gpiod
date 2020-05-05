#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <gpiod.h>

#include "log.h"
#include "gpiomonitor.h"
#include "exec.h"

#define DEBOUNCING (1 << 16)

typedef int mutex_t;
#define mutex_lock(mut) do{}while(0)
#define mutex_unlock(mut) do{}while(0)

const char str_gpiod[] = "gpiod";

typedef struct gpio_handler_s gpio_handler_t;
struct gpio_handler_s
{
	void *ctx;
	handler_t handler;
	gpio_handler_t *next;
};

typedef struct gpio_s gpio_t;
struct gpio_s
{
	struct gpiod_line *handle;
	char *name;
	struct pollfd *poll_set;
	gpio_t *next;
	gpio_handler_t *handlers;
	int fd;
	int id;
	int chipid;
	int last;
};

typedef struct gpiochip_s gpiochip_t;
struct gpiochip_s
{
	struct gpiod_chip *handle;
	gpiochip_t *next;
	int id;
};

static gpiochip_t *g_gpiochip = NULL;
static gpio_t *g_gpios = NULL;
static int g_run = 1;

int gpiod_addchip(struct gpiod_chip *handle)
{
	const char *label = gpiod_chip_label(handle);
	if (label == NULL)
	{
		err("gpiod: chip label NULL");
		return -1;
	}
	gpiochip_t *chip = g_gpiochip;
	while (chip != NULL)
	{
		if (!strcmp(gpiod_chip_label(chip->handle),label))
			break;
		chip = chip->next;
	}
	if (chip != NULL)
		return chip->id;

	chip = calloc(1, sizeof(*chip));
	chip->handle = handle;
	if (g_gpiochip != NULL)
		chip->id = g_gpiochip->id + 1;
	chip->next = g_gpiochip;
	g_gpiochip = chip;
	return chip->id;
}

int gpiod_addhandler(int gpioid, void *ctx, handler_t callback)
{
	gpio_t *gpio = g_gpios;
	while (gpio != NULL && gpio->id != gpioid) gpio = gpio->next;
	if (gpio == NULL)
	{
		err("gpiod: set handler, gpio %d not found", gpioid);
		return -1;
	}

	gpio_handler_t *handler = NULL;
	handler = calloc(1, sizeof(*handler));
	handler->ctx = ctx;
	handler->handler = callback;
	handler->next = gpio->handlers;
	gpio->handlers = handler;

	return 0;
}

int gpiod_setline(int chipid, struct gpiod_line *handle, const char *name)
{
	uint32_t handleflags = 0;
	gpiochip_t *chip = g_gpiochip;
	while (chip != NULL && chip->id != chipid) chip = chip->next;
	if (chip == NULL)
	{
		err("gpiod: chip %d not found", chipid);
		return -1;
	}

	const char *consumer = gpiod_line_consumer(handle);
	if (consumer != NULL)
	{
		if (!strcmp(consumer, str_gpiod))
		{
			int line = gpiod_line_offset(handle);
			gpio_t *gpio = g_gpios;
			while (gpio != NULL)
			{
				if (line == gpiod_line_offset(gpio->handle))
					return gpio->id;
				gpio = gpio->next;
			}
		}
	}
	if (consumer != NULL)
	{
		err("gpiod: line %d already used by %s", gpiod_line_offset(handle), consumer);
		return -1;
	}
	struct gpiod_line_request_config config = {
		.consumer = str_gpiod,
		.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES,
		.flags = 0,
	};
	if (gpiod_line_request(handle, &config, 0) < 0)
	{
		err("gpiod: request line %d error", gpiod_line_offset(handle));
		return -1;
	}
	int fd = gpiod_line_event_get_fd(handle);
	if (fd < 0)
	{
		gpiod_line_release(handle);
		err("gpiod: line %d configuration error", gpiod_line_offset(handle));
		return -1;
	}

	gpio_t *gpio = calloc(1, sizeof(*gpio));
	gpio->chipid = chip->id;
	gpio->handle = handle;
	gpio->fd = fd;
	if (name != NULL)
		gpio->name = strdup(name);

	if (g_gpios)
		gpio->id = g_gpios->id +1;
	gpio->next = g_gpios;
	g_gpios = gpio;

	return gpio->id;
}

const char *gpiod_name(int gpioid)
{
	gpio_t *gpio = g_gpios;
	while (gpio != NULL && gpio->id != gpioid) gpio = gpio->next;
	if (gpio == NULL)
	{
		err("gpiod: gpio %d not found", gpioid);
		return NULL;
	}
	return gpio->name;
}

static int gpiod_setpoll(struct pollfd *poll_set, int numpoll)
{
	int numfds = 0;
	mutex_lock(&g_gpios->lock);
	gpio_t *gpio = g_gpios;
	while (gpio != NULL)
	{
		if (numfds < numpoll)
		{
			if (! (gpio->last & DEBOUNCING))
			{
				poll_set[numfds].fd = gpio->fd;
				poll_set[numfds].events = POLLIN;
				poll_set[numfds].revents = 0;
				gpio->poll_set = &poll_set[numfds];
				numfds++;
			}
			else
				gpio->last &= ~DEBOUNCING;
		}
		gpio = gpio->next;
	}
	mutex_unlock(&g_gpios->lock);
	return numfds;
}

static int gpiod_dispatch(gpio_t *gpio, struct gpiod_line_event *event)
{
	gpio_handler_t *handler = gpio->handlers;
	int line = gpiod_line_offset(gpio->handle);
	int value = gpiod_line_get_value(gpio->handle);
	if (value == 1)
		event->event_type = GPIOD_LINE_EVENT_RISING_EDGE;
	else if (value == 0)
		event->event_type = GPIOD_LINE_EVENT_FALLING_EDGE;
	else
		return 0;

	dbg("gpiod: line %d %s", line,
		(event->event_type == GPIOD_LINE_EVENT_RISING_EDGE)?
			"rising":"falling");

	if ((gpio->last & ~DEBOUNCING) == event->event_type)
		return 0;
	gpio->last = event->event_type | DEBOUNCING;
	while (handler != NULL)
	{
		handler->handler(handler->ctx, gpio->chipid, line, event);
		handler = handler->next;
	}
	return 1;
}
static int gpiod_check()
{
	gpio_t *gpio = g_gpios;
	while (gpio != NULL)
	{
		struct gpiod_line_event event;
		int ret = gpiod_line_get_value(gpio->handle);
		if (ret == 1)
			event.event_type = GPIOD_LINE_EVENT_RISING_EDGE;
		else if (ret == 0)
			event.event_type = GPIOD_LINE_EVENT_FALLING_EDGE;
		gpiod_dispatch(gpio, &event);
		gpio = gpio->next;
	}
}

int gpiod_monitor()
{
	struct pollfd poll_set[MAX_GPIOS];
	int numfds = 0;
	int debouncing = 0;

	gpiod_check();

	while(g_run)
	{
		int ret;

		numfds = gpiod_setpoll(poll_set, MAX_GPIOS);

		ret = poll(poll_set, numfds, debouncing);
		debouncing = -1;
		if (ret > 0)
		{
			ret = 0;
			gpio_t *gpio = g_gpios;
			while (gpio != NULL)
			{
				if (gpio->poll_set->revents == POLLIN)
				{
					struct gpiod_line_event event;
					ret = gpiod_line_event_read_fd(gpio->fd, &event);
					if (ret < 0)
						break;
					ret += gpiod_dispatch(gpio, &event);
					gpio->poll_set->revents = 0;
				}
				gpio = gpio->next;
			}
			if (ret > 0)
			{
				debouncing = 500;
			}
		}
	}
}

void gpiod_stop()
{
	g_run = 0;
}

void gpiod_free()
{
	gpio_t *gpio = g_gpios;
	while (gpio != NULL)
	{
		gpio_t *next = gpio->next;
		gpio_handler_t *handler = gpio->handlers;
		gpiod_line_release(gpio->handle);
		while (handler != NULL)
		{
			gpio_handler_t *next = handler->next;
			exec_free(handler->ctx);
			free(handler);
			handler = next;
		}
		if (gpio->name != NULL)
			free(gpio->name);
		free(gpio);
		gpio = next;
	}

	gpiochip_t *chip = g_gpiochip;
	while (chip != NULL)
	{
		gpiochip_t *next = chip->next;
		gpiod_chip_close(chip->handle);
		free(chip);
		chip = next;
	}
}

#ifdef TEST
void test_handler(void *ctx, int chipid, int line, gpiod_event_t event)
{
	dbg("event");
}
int main(int argc, char **argv)
{
	int line = atoi(argv[1]);

	struct gpiod_chip *chiphandle = gpiod_chip_open("/dev/gpiochip0");
	int chipid = gpiod_addchip(chiphandle);
	struct gpiod_line *handle;
	handle = gpiod_chip_get_line(chiphandle, line);
	int gpioid = gpiod_setline(chipid, handle);
	gpiod_addhandler(gpioid, NULL, test_handler);
	gpiod_monitor();
	gpiod_stop();
	return 0;
}
#endif