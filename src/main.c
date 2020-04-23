#include <unistd.h>

#include "log.h"
#include "gpiomonitor.h"
#include "exec.h"
#include "rules.h"

#define PACKAGEVERSION PACKAGE "/" VERSION

int main(int argc, char **argv)
{
	const char * sysconfdir = SYSCONFDIR;
	int daemonize = 0;

	int opt;
	do
	{
		opt = getopt(argc, argv, "hDVR:");
		switch (opt)
		{
			case 'h':
			return -1;
			case 'V':
				printf("%s\n",PACKAGEVERSION);
			return -1;
			case 'D':
				daemonize = 1;
			break;
			case 'R':
				sysconfdir = optarg;
			break;
			default:
			break;
		}
	} while(opt != -1);

	dbg("sysconfdir %s", sysconfdir);
	if (rules_set(sysconfdir) == 0)
	{
		if (daemonize && fork() != 0)
		{
			return 0;
		}

		gpiod_monitor();
		gpiod_free();
	}
	return 0;
}
