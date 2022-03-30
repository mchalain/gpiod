/*****************************************************************************
 * exec.c
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
#include <sys/wait.h>

#include "log.h"
#include "gpiomonitor.h"

typedef struct exec_s exec_t;
struct exec_s
{
	int rootfd;
	char *cgipath;
	char **env;
};

static const char str_rising[] = "rising";
static const char str_falling[] = "falling";
static const char str_empty[] = "";
#define NUMENVS 3
static const char str_GPIOENV[] = "GPIO=%.2d";
static const char str_CHIPENV[] = "CHIP=%.2d";
static const char str_NAMEENV[] = "NAME=%.18s";
static const char str_ACTIONENV[] = "ACTION=%.7s";

void *exec_create(int rootfd, const char *cgipath, char **env, int nbenvs)
{
	exec_t *ctx = calloc(1, sizeof(*ctx));
	ctx->rootfd = rootfd;
	if (cgipath != NULL)
	{
		ctx->cgipath = strdup(cgipath);
	}
	else
	{
		err("exec %s not found", cgipath);
		return NULL;
	}
	ctx->env = calloc(nbenvs + NUMENVS + 1, sizeof(char *));
	for (int i = 0; i < nbenvs; i++)
	{
		ctx->env[i + NUMENVS] = strdup(env[i]);
	}
	ctx->env[nbenvs + NUMENVS] = NULL;
	return ctx;
}

void exec_run(void *arg, int chipid, int gpioid, struct gpiod_line_event *event)
{
	const exec_t *ctx = (const exec_t *)arg;
#ifdef ENABLE_CGISTDFILE
	int tocgi[2];
	int fromcgi[2];

	if (pipe(tocgi) < 0)
		return -1;
	if (pipe(fromcgi))
		return -1;
#endif

	pid_t pid = fork();
	if (pid == 0) /* into child */
	{
#ifdef ENABLE_CGISTDFILE
		int flags;
		flags = fcntl(tocgi[0],F_GETFD);
		fcntl(tocgi[0],F_SETFD, flags | FD_CLOEXEC);
		flags = fcntl(fromcgi[1],F_GETFD);
		fcntl(fromcgi[1],F_SETFD, flags | FD_CLOEXEC);
		/* send data from server to the stdin of the cgi */
		close(tocgi[1]);
		dup2(tocgi[0], STDIN_FILENO);
		close(tocgi[0]);
		/* send data from the stdout of the cgi to server */
		close(fromcgi[0]);
		dup2(fromcgi[1], STDOUT_FILENO);
		close(fromcgi[1]);
#endif

#ifdef USE_GLIBC
# define STRNDUP strndupa
# define STRDUP strdupa
# define SPRINTF asprintf
#else
# define STRNDUP strndup
# define STRDUP strdup
#endif

		int argc = 0;
		char *argv[10];
		argv[argc] =  STRDUP(ctx->cgipath);
		argc++;
		while ( argc < 10 && argv[argc - 1] != NULL)
		{
			argv[argc] = strchr(argv[argc - 1], ' ');
			if (argv[argc] != NULL)
			{
				*argv[argc] = '\0';
				argv[argc]++;
			}
			argc++;
		}

		char **env = ctx->env;
		env[0] = STRDUP(str_GPIOENV);
		sprintf(env[0], str_GPIOENV, gpiod_line(gpioid));
		env[1] = STRDUP(str_CHIPENV);
		sprintf(env[1], str_CHIPENV, chipid);
		const char *gpioname = NULL;
		env[2] = malloc(25);
		gpioname = gpiod_name(gpioid);
		if (gpioname == NULL)
		{
			gpioname = str_empty;
		}
		snprintf(env[2], 25, str_NAMEENV, gpioname);
		const char *eventstr = NULL;
		if (event->event_type == GPIOD_LINE_EVENT_RISING_EDGE)
			eventstr = str_rising;
		else
			eventstr = str_falling;
		env[3] = malloc(sizeof(str_ACTIONENV) + sizeof(str_falling));
		sprintf(env[3], str_ACTIONENV, eventstr);

		setbuf(stdout, 0);
		sched_yield();

		int ret = setsid();

		/**
		 * cgipath is absolute, but in fact execveat runs in docroot.
		 */
		int rootfd = ctx->rootfd;
		char *cgipath = argv[0];

		if (faccessat(rootfd, cgipath, X_OK, 0))
		{
			err("exec %s not found", cgipath);
			exit(-1);
		}

		/**
		 * on some system the setsid is not enough
		 * the parent must wait the child.
		 * With a second fork, the third process will live alone,
		 * and the parent wait only the second one.
		 */
		if (vfork() != 0)
			exit(0);

#ifdef USE_EXECVEAT
		execveat(rootfd, cgipath, argv, env);
#elif !defined(__UCLIBC__)
		int scriptfd = openat(rootfd, cgipath, O_PATH);
		close(rootfd);
		if (scriptfd != -1)
		{
			dbg("gpiod: event %s %s", argv[0], argv[1]);
			fexecve(scriptfd, argv, env);
		}
#elif defined(__USE_GNU)
		execvpe(cgipath, argv, env);
#else
		execve(cgipath, argv, env);
#endif
		err("gpiod: cgi error: %s", strerror(errno));
		exit(0);
	}
	else if (pid > 0)
	{
#ifdef ENABLE_CGISTDFILE
		/* keep only input of the pipe */
		close(tocgi[0]);
		/* keep only output of the pipe */
		close(fromcgi[1]);
#endif
		/**
		 * wait the first fork.
		 */
		wait(NULL);
	}
	return;
}

void exec_free(void *arg)
{
	exec_t *ctx = (exec_t *)arg;
	if (ctx->cgipath)
		free(ctx->cgipath);
	free(ctx);
}
