
USE_GLIBC=y
USE_EXECVEAT=n
ENABLE_CGISTDFILE=n
LIBCONFIG=y

ifneq ($(TEST),y)
sbin-y=gpiod
endif
gpiod_SOURCES+=main.c
gpiod_SOURCES+=daemonize.c
gpiod_SOURCES+=gpiomonitor.c
gpiod_SOURCES+=exec.c
gpiod_SOURCES+=led.c
gpiod_SOURCES+=export.c
gpiod_SOURCES-$(LIBCONFIG)+=rules_conf.c
gpiod_LIBS+=gpiod pthread
gpiod_LIBS-$(LIBCONFIG)+=config
gpiod_CFLAGS-$(DEBUG)+=-DDEBUG -g -O0

bin-$(TEST)+=test_gpiomonitor
test_gpiomonitor_SOURCES+=gpiomonitor.c
test_gpiomonitor_CFLAGS+=-DTEST


