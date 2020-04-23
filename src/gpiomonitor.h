#ifndef __GPIOMONITOR_H__
#define __GPIOMONITOR_H__

#include <gpiod.h>

typedef void (*handler_t)(void *ctx, int chipid, int line, struct gpiod_line_event *event);

int gpiod_addchip(struct gpiod_chip *handle);
int gpiod_addhandler(int gpioid, void *ctx, handler_t callback);
int gpiod_setline(int chipid, struct gpiod_line *handle);
int gpiod_monitor();
void gpiod_stop();
void gpiod_free();

#endif
