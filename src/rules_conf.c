/*****************************************************************************
 * rules_conf.c
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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include <libconfig.h>

#include "log.h"
#include "gpiomonitor.h"
#include "exec.h"
#include "led.h"
#include "export.h"
#include "rules.h"

static int g_rootfd = AT_FDCWD;

static void *rules_ledrule(struct gpiod_chip *chiphandle, config_setting_t *led, int bled)
{
	void *ctx = NULL;
	struct gpiod_line *handle = NULL;
	if (config_setting_is_number(led))
	{
		int line = config_setting_get_int(led);
		handle = gpiod_chip_get_line(chiphandle, line);
	}
	else if (config_setting_type(led) == CONFIG_TYPE_STRING)
	{
		const char *name = config_setting_get_string(led);
		handle = gpiod_chip_find_line(chiphandle, name);
	}
	if (handle != NULL)
	{
		ctx = led_create(handle, bled);
	}
	return ctx;
}

static void *rules_exportrule(int gpioid, config_setting_t *export)
{
	void *ctx = NULL;
	const char *url = NULL;
	const char *format = "raw";
	if (config_setting_type(export) == CONFIG_TYPE_STRING)
	{
		url = config_setting_get_string(export);
	}
	else if (config_setting_is_group(export))
	{
		config_setting_lookup_string(export, "url", &url);
		config_setting_lookup_string(export, "format", &format);
	}
	if (url != NULL)
		ctx = export_create(g_rootfd, url, format, gpioid);
	return ctx;
}

static int _rules_get_inputoptions(config_setting_t *gpiosetting)
{
	int options = 0;
#ifdef GPIOD_DEFAULT_PULL_UP
	options |= GPIOD_LINE_OPTION_PULL_UP;
#endif
	int ret = CONFIG_FALSE;
	int pull = 0;

	ret = config_setting_lookup_bool(gpiosetting, "pull_up", &pull);
	if (ret == CONFIG_TRUE)
	{
		if (pull)
			options |= GPIOD_LINE_OPTION_PULL_UP;
		else
			options &= ~GPIOD_LINE_OPTION_PULL_UP;
	}
	else
	{
		pull = 0;
		config_setting_lookup_bool(gpiosetting, "pull_down", &pull);
		if (pull)
			options |= GPIOD_LINE_OPTION_PULL_DOWN;
	}
	return options;
}

int rules_getgpio(config_setting_t *gpiosetting, int chipid, struct gpiod_chip *chiphandle, const char *name)
{
	int gpioid = -1;
	int options = 0;
	struct gpiod_line *handle = NULL;

	if (handle == NULL && gpiosetting != NULL)
	{
		int line = -1;
		if (config_setting_is_number(gpiosetting))
			line = config_setting_get_int(gpiosetting);
		else if (config_setting_type(gpiosetting) == CONFIG_TYPE_STRING)
		{
			line = strtol(config_setting_get_string(gpiosetting), NULL, 10);
		}
		else if (config_setting_is_group(gpiosetting))
		{
			config_setting_lookup_int(gpiosetting, "offset", &line);

			config_setting_lookup_string(gpiosetting, "name", &name);

			int default_direction = 0;
			config_setting_lookup_bool(gpiosetting, "default", &default_direction);
			if (default_direction)
				options |= GPIOD_LINE_OPTION_DEFAULT;

			int output = 0;
			config_setting_lookup_bool(gpiosetting, "output", &output);
			if (output)
				options |= GPIOD_LINE_OPTION_OUTPUT;

			if (!default_direction && !output)
			{
				options |= _rules_get_inputoptions(gpiosetting);
			}
		}

		if (line > -1 && chiphandle != NULL)
		{
			handle = gpiod_chip_get_line(chiphandle, line);
		}
	}

	if (handle == NULL && name != NULL)
		handle = gpiod_chip_find_line(chiphandle, name);

	if (handle != NULL)
	{
		gpioid = gpiod_setline(chipid, handle, name, options);
	}
	return gpioid;
}

static int rules_parserule(config_setting_t *iterator)
{
	int gpioid[64] = {-1};
	memset(gpioid, -1, sizeof(gpioid));
	int ngpioids = 0;
	struct
	{
		void *ctx;
		handler_t run;
		free_ctx_t free;
		int action;
	} handlers[3];
	int nhandlers = 0;
	if (iterator)
	{
		int chipid = -1;
		const char *chipname = NULL;
		struct gpiod_chip *chiphandle = NULL;
		const char *device = NULL;

		/**
		 * gpiochip is mandatory
		 * It may be find with the "device" path or by "chipname"
		 * The chipname is available from the device tree
		 * Example:
		 *  device = "/dev/gpiochip0";
		 * or
		 *  chipname = "gpio0";
		 */
		config_setting_lookup_string(iterator, "device", &device);
		if (device != NULL)
		{
			chiphandle = gpiod_chip_open(device);
		}
		config_setting_lookup_string(iterator, "chipname", &chipname);
		if (chiphandle == NULL && chipname != NULL)
		{
			chiphandle = gpiod_chip_open_by_label(chipname);
		}
		if (chiphandle != NULL)
		{
			chipid = gpiod_addchip(chiphandle);
		}
		if (chipid < 0)
		{
			err("gpiod: chip not found");
			return -1;
		}
		/**
		 * The GPIO chip is ready
		 */

		/**
		 * Set the line of the GPIO chip to attach the handlers.
		 * Look for the line by its number or by the name available
		 * in the device tree.
		 * Example
		 *  name = "gpio0_1";
		 * or
		 *  line = "1";
		 * or
		 *  line = 1;
		 * or
		 *  lines = [1, 2, 3];
		 * or
		 *  lines = ["1","2","3"];
		 */
		const char *name = NULL;
		config_setting_lookup_string(iterator, "name", &name);
		gpioid[ngpioids] = rules_getgpio(config_setting_lookup(iterator, "line"),
								chipid, chiphandle, name);
		if (gpioid[ngpioids] > -1)
			ngpioids++;
		config_setting_t *lines = config_setting_lookup(iterator, "lines");
		if (lines != NULL && config_setting_is_array(lines))
		{
			int i;
			int length = config_setting_length(lines);
			length = (length > 64)?64:length;
			for (i = 0; i < length; i++)
			{
				gpioid[ngpioids] = rules_getgpio(config_setting_get_elem(lines, i),
										chipid, chiphandle, name);
				if (gpioid[ngpioids] > -1)
					ngpioids++;
			}
		}
		/**
		 * At least gpio line is set
		 */

		/**
		 * Configure the handler to attach to the gpio line.
		 * Three kinds of handler are available:
		 *  - exec: to launch an application
		 *  - led: to change the state of an output GPIO
		 *  - bled: to add a blink handler on an output GPIO
		 * Warning: led and bled state change on each event
		 * The action may be:
		 *  - falling
		 *  - rising
		 *  - the both
		 * Example:
		 *  action = "rising";
		 *  exec = "/usr/bin/mytest";
		 * or
		 *  action = "both"
		 *  led = 4;
		 * or
		 *  bled = "gpio0_4";
		 */
		handlers[nhandlers].action = GPIOD_LINE_EVENT_RISING_EDGE | GPIOD_LINE_EVENT_FALLING_EDGE;
		config_setting_t *action;
		action = config_setting_lookup(iterator, "action");
		if (action != NULL)
		{
			const char *string;
			if (config_setting_is_number(action))
				handlers[nhandlers].action = config_setting_get_int(action);
			switch (config_setting_type(action))
			{
			case CONFIG_TYPE_INT:
				handlers[nhandlers].action = config_setting_get_int(action);
			break;
			case CONFIG_TYPE_STRING:
				string = config_setting_get_string(action);
				if (!strcmp(string, "falling"))
					handlers[nhandlers].action = GPIOD_LINE_EVENT_FALLING_EDGE;
				if (!strcmp(string, "rising"))
					handlers[nhandlers].action = GPIOD_LINE_EVENT_RISING_EDGE;
				if (!strcmp(string, "both"))
					handlers[nhandlers].action = GPIOD_LINE_EVENT_RISING_EDGE | GPIOD_LINE_EVENT_FALLING_EDGE;
			break;
			}
			if (handlers[nhandlers].action < 0 ||
				handlers[nhandlers].action > (GPIOD_LINE_EVENT_RISING_EDGE | GPIOD_LINE_EVENT_FALLING_EDGE))
				handlers[nhandlers].action = GPIOD_LINE_EVENT_RISING_EDGE | GPIOD_LINE_EVENT_FALLING_EDGE;
		}

		char **env = NULL;
		int nbenvs = 0;

		const char *exec = NULL;
		config_setting_lookup_string(iterator, "exec", &exec);
		if (exec != NULL)
		{
			handlers[nhandlers].ctx = exec_create(g_rootfd, exec, env, nbenvs);
			handlers[nhandlers].run = &exec_run;
			handlers[nhandlers].free = &exec_free;
			if (handlers[nhandlers].ctx != NULL)
				nhandlers++;
		}

		config_setting_t *led;
		led = config_setting_lookup(iterator, "led");
		if (led != NULL)
		{
			handlers[nhandlers].ctx = rules_ledrule(chiphandle, led, 0);
			handlers[nhandlers].run = &led_run;
			handlers[nhandlers].free = &led_free;
			if (handlers[nhandlers].ctx != NULL)
				nhandlers++;
		}
		led = config_setting_lookup(iterator, "bled");
		if (led != NULL)
		{
			handlers[nhandlers].ctx = rules_ledrule(chiphandle, led, 1);
			handlers[nhandlers].run = &led_run;
			handlers[nhandlers].free = &led_free;
			if (handlers[nhandlers].ctx != NULL)
				nhandlers++;
		}

		config_setting_t *export;
		export = config_setting_lookup(iterator, "export");
		if (export != NULL)
		{
			handlers[nhandlers].ctx = rules_exportrule(gpioid[0], export);
			handlers[nhandlers].run = &export_run;
			handlers[nhandlers].free = &export_free;
			if (handlers[nhandlers].ctx != NULL)
				nhandlers++;
		}

	}
	int ret = -1;
	int i;
	for (i = 0; i < nhandlers; i++)
	{
		int j = 0;
		while (gpioid[j] > -1)
		{
			ret = gpiod_addhandler(gpioid[j], handlers[0].action, handlers[i].ctx, handlers[i].run, handlers[i].free);
			j++;
		}
	}
	return ret;
}

void rules_setroot(char *rootpath)
{
	g_rootfd = open(rootpath, O_PATH | O_DIRECTORY);
	if (g_rootfd == -1)
	{
		g_rootfd = AT_FDCWD;
	}
}

int rules_parse(const char *filepath)
{
	int ret = -1;
	config_t configfile = {0};
	memset(&configfile, 0, sizeof(configfile));

	config_init(&configfile);
	ret = config_read_file(&configfile, filepath);
	if (ret != CONFIG_TRUE)
	{
		err("gpiod: rules %s error: %s", filepath, config_error_text(&configfile));
		return -1;
	}

	config_setting_t *configrules = config_lookup(&configfile, "rules");
	if (configrules)
	{
		int count = config_setting_length(configrules);
		int i;

		for (i = 0; i < count; i++)
		{
			config_setting_t *iterator = config_setting_get_elem(configrules, i);
			ret = rules_parserule(iterator);
			if (ret < -1)
				break;
		}
	}
	config_destroy(&configfile);
	return ret;
}

static const char *rulespath[] =
{
	"rules.d",
	"gpiod/rules.d",
	"gpiod.d",
	NULL
};

int rules_set(const char *sysconfdir)
{
	DIR *rulesdir = NULL;
	int rulesindex = 0;
	while (rulespath[rulesindex] != NULL)
	{
		char *path = NULL;
		if (asprintf(&path, "%s/%s", sysconfdir, rulespath[rulesindex]) == -1 ||
			 path == NULL)
			break;
		rulesdir = opendir(path);
		free(path);
		if (rulesdir != NULL)
			break;
		rulesindex++;
	}
	if (rulesdir == NULL)
	{
		err("gpiod: rules.d directory not found in %s", sysconfdir);
		return -1;
	}

	int ret = -1;
	struct dirent *item = NULL;
	item = readdir(rulesdir);
	if (item == NULL)
		err("readdir %s", strerror(errno));
	while(item != NULL)
	{
		if (item->d_name[0] == '.')
		{
			item = readdir(rulesdir);
			continue;
		}
		char *filepath;
		ret = asprintf(&filepath, "%s/%s/%s", sysconfdir, rulespath[rulesindex], item->d_name);
		if (ret > 0)
		{
			ret = rules_parse(filepath);
			if (ret < 0)
			{
				err("error on %s", filepath);
				free(filepath);
				break;
			}
			free(filepath);
		}
		item = readdir(rulesdir);
	}
	closedir(rulesdir);
	return ret;
}
