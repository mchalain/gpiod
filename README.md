# GPIOD: gpio monitoring daemon.

gpiod is able to run a script when a gpio event occures. It uses rules' files to configure each gpio and script, like udev.

## build gpiod

``` shell
$ make prefix=/usr sysconfdir=/etc/gpiod defconfig
$ make
$ sudo make install
```

## rules

The rules files must be placed inside the $SYSCONFDIR/rules.d directory.
"gpiod" is able to read each file and to load up to 64 gpio rules.

``` config
rules=({
	device = "/dev/gpiochip0";
	line = " 26";
	exec = "/sbin/shutdown";
	},{
	chipname = "pinctrl-bcm2835";
	name = "rfkill";
	exec = "/etc/init.d/S40wifi";
	});
```

The script is called with environment variables:
 * GPIO the gpio line
 * CHIP a chip id
 * NAME the gpio name
and with an argument "rising" or "falling"

``` shell
#!/bin/sh

echo "$CHIP $GPIO $NAME $1" >> /tmp/test.log
```

