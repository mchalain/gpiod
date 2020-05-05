#ifndef __LED_H__
#define __LED_H__

void *led_create(struct gpiod_line *handle, int bare);
void led_run(void *arg, int chipid, int gpioid, struct gpiod_line_event *event);
void led_free(void *arg);

#endif
