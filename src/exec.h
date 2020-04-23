#ifndef __EXEC_H__
#define __EXEC_H__

void *exec_create(int rootfd, const char *cgipath, const char *gpioname, char **env, int nbenvs);
void exec_run(void *arg, int chipid, int line, struct gpiod_line_event *event);
void exec_free(void *arg);

#endif
