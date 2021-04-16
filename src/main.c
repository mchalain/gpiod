/*****************************************************************************
 * main.c
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "log.h"
#include "gpiomonitor.h"
#include "exec.h"
#include "rules.h"
#include "daemonize.h"

#define PACKAGEVERSION PACKAGE_NAME "/" PACKAGE_VERSION

void display_help(char * const *argv)
{
	fprintf(stderr, PACKAGE_NAME" "PACKAGE_VERSION" build: "__DATE__" "__TIME__"\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s [-h][-D][-K]\n", argv[0]);
	fprintf(stderr, "\t-h \t\tshow this help and exit\n");
	fprintf(stderr, "\t-D \t\tto daemonize the server\n");
	fprintf(stderr, "\t-K \t\tto kill the current daemon\n");
	fprintf(stderr, "\t-p <pidfile> \t\tto set the pid file of the daemon\n");
	fprintf(stderr, "\t-R <dir> \t\tto set the rules directory\n");
}

#ifdef HAVE_SIGACTION
static void handler(int sig, siginfo_t *si, void *arg)
#else
static void handler(int sig)
#endif
{
	gpiod_stop();
}

#define DAEMONIZE 0x01
#define KILLDAEMON 0x02
int main(int argc, char **argv)
{
	const char * sysconfdir = SYSCONFDIR;
	int mode = 0;
	char *pidfile = NULL;

	int opt;
	do
	{
		opt = getopt(argc, argv, "hDKVR:p:");
		switch (opt)
		{
			case 'h':
				display_help(argv);
			return -1;
			case 'V':
				printf("%s\n",PACKAGEVERSION);
			return -1;
			case 'p':
				pidfile = optarg;
			break;
			case 'D':
				mode |= DAEMONIZE;
			break;
			case 'K':
				mode |= KILLDAEMON;
			break;
			case 'R':
				sysconfdir = optarg;
			break;
			default:
			break;
		}
	} while(opt != -1);

	if (mode & KILLDAEMON)
	{
		killdaemon(pidfile);
		return 0;
	}
	if (mode & DAEMONIZE && daemonize(pidfile) == -1)
	{
		return 0;
	}

	dbg("sysconfdir %s", sysconfdir);
	if (rules_set(sysconfdir) == 0)
	{
#ifdef HAVE_SIGACTION
		struct sigaction action;
		action.sa_flags = SA_SIGINFO;
		sigemptyset(&action.sa_mask);
		action.sa_sigaction = handler;
		sigaction(SIGTERM, &action, NULL);
		sigaction(SIGINT, &action, NULL);

		struct sigaction unaction;
		unaction.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &action, NULL);
#else
		signal(SIGTERM, handler);
		signal(SIGINT, handler);

		signal(SIGPIPE, SIG_IGN);
#endif

		gpiod_monitor();
		gpiod_free();
	}
	killdaemon(pidfile);
	return 0;
}
