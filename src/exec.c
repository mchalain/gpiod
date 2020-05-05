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

typedef struct exec_s exec_t;
struct exec_s
{
	int rootfd;
	char *cgipath;
	char **env;
	char *gpioname;
};

static const char str_rising[] = "rising";
static const char str_falling[] = "falling";
static const char str_empty[] = "";
#define NUMENVS 3
static const char str_GPIOENV[] = "GPIO=%.2d";
static const char str_CHIPENV[] = "CHIP=%.2d";
static const char str_NAMEENV[] = "NAME=%s";

void *exec_create(int rootfd, const char *cgipath, char **env, int nbenvs)
{
	exec_t *ctx = calloc(1, sizeof(*ctx));
	ctx->rootfd = rootfd;
	if (cgipath != NULL && !faccessat(rootfd, cgipath, X_OK, 0))
	{
		ctx->cgipath = strdup(cgipath);
	}
	else
	{
		err("exec %s not found", cgipath);
		return NULL;
	}
	ctx->env = calloc(nbenvs + NUMENVS + 1, 4);
	int i = 0;
	for (; i < nbenvs; i++)
	{
		ctx->env[i + NUMENVS] = strdup(env[i]);
	}
	ctx->env[i + NUMENVS] = NULL;
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

		const char *eventstr = NULL;
		if (event->event_type == GPIOD_LINE_EVENT_RISING_EDGE)
			eventstr = str_rising;
		else
			eventstr = str_falling;
		char *const argv[3] = { STRDUP(ctx->cgipath), STRDUP(eventstr), NULL };
		char **env = ctx->env;
		env[0] = STRNDUP(str_GPIOENV, sizeof(str_GPIOENV));
		sprintf(env[0], str_GPIOENV, gpiod_line(gpioid));
		env[1] = STRNDUP(str_CHIPENV, sizeof(str_CHIPENV));
		sprintf(env[1], str_CHIPENV, chipid);
		const char *gpioname = NULL;
		env[2] = malloc(25);
		gpioname = gpiod_name(gpioid);
		if (gpioname == NULL)
		{
			gpioname = str_empty;
		}
		snprintf(env[2], 25, str_NAMEENV, gpioname);

		setbuf(stdout, 0);
		sched_yield();

		setsid();
		/**
		 * cgipath is absolute, but in fact execveat runs in docroot.
		 */
#ifdef USE_EXECVEAT
		execveat(ctx->rootfd, ctx->cgipath, argv, env);
#else
		int scriptfd = openat(ctx->rootfd, ctx->cgipath, O_PATH);
		close(ctx->rootfd);
		if (scriptfd > 0)
		{
			dbg("gpiod: event %s %s", argv[0], argv[1]);
			fexecve(scriptfd, argv, env);
		}
#endif
		err("gpiod: cgi error: %s", strerror(errno));
		exit(0);
	}
#ifdef ENABLE_CGISTDFILE
	else
	{
		/* keep only input of the pipe */
		close(tocgi[0]);
		/* keep only output of the pipe */
		close(fromcgi[1]);
	}
#endif
	return;
}

void exec_free(void *arg)
{
	exec_t *ctx = (exec_t *)arg;
	if (ctx->cgipath)
		free(ctx->cgipath);
	if (ctx->gpioname)
		free(ctx->gpioname);
	free(ctx);
}
