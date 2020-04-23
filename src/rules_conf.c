#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include <libconfig.h>

#include "log.h"
#include "gpiomonitor.h"
#include "exec.h"
#include "rules.h"

static int g_rootfd = AT_FDCWD;

static int rules_parserule(config_setting_t *iterator)
{
	int ret = -1;
	int chipid = -1;
	int gpioid = -1;
	char **env = NULL;
	int nbenvs = 0;
	if (iterator)
	{
		const char *chipname = NULL;
		struct gpiod_chip *chiphandle = NULL;
		const char *device = NULL;
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

		const char *name = NULL;
		struct gpiod_line *handle = NULL;
		int line = -1;
		config_setting_lookup_int(iterator, "line", &line);
		if (line > -1 && chiphandle != NULL)
		{
			handle = gpiod_chip_get_line(chiphandle, line);
		}
		config_setting_lookup_string(iterator, "name", &name);
		if (handle == NULL && name != NULL && chiphandle != NULL)
		{
			handle = gpiod_chip_find_line(chiphandle, name);
		}
		if (handle != NULL)
		{
			name = gpiod_line_name(handle);
			gpioid = gpiod_setline(chipid, handle);
		}

		const char *exec = NULL;
		config_setting_lookup_string(iterator, "exec", &exec);
		if (exec != NULL && gpioid > -1)
		{

			void *ctx = NULL;
			ctx = exec_create(g_rootfd, exec, name, env, nbenvs);
			ret = gpiod_addhandler(gpioid, ctx, exec_run);
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
	config_t configfile;

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
	NULL
};
int rules_set(const char *sysconfdir)
{
	int sysconffd = open(sysconfdir, O_PATH | O_DIRECTORY);
	int rulesfd;
	int rulesindex = 0;
	while (rulespath[rulesindex] != NULL)
	{
		rulesfd = openat(sysconffd, rulespath[rulesindex], O_PATH | O_DIRECTORY);
		if (rulesfd > 0)
			break;
		rulesindex++;
	}
	if (rulesfd < 0)
	{
		err("gpiod: rules.d directory not found in %s", sysconfdir);
		return -1;
	}

	int nbitems;
	struct dirent **items;
	nbitems = scandirat(rulesfd, ".", &items, NULL, alphasort);

	int ret = 0;
	int i;
	for (i = 0; i < nbitems; i++)
	{
		if (items[i]->d_name[0] == '.')
			continue;
		char *filepath;
		ret = asprintf(&filepath, "%s/%s/%s", sysconfdir, rulespath[rulesindex], items[i]->d_name);
		if (ret > 0)
		{
			ret = rules_parse(filepath);
			if (ret < 0)
				break;
			free(filepath);
		}
	}
	return ret;
}
