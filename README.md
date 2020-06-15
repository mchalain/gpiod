# GPIOD: gpio monitoring daemon.

gpiod is able to run a script when a gpio event occures. It uses rules' files to configure each gpio and script, like udev.

## build gpiod

"gpiod" depends on:
 * libgpiod: https://www.kernel.org/pub/software/libs/libgpiod
 * libconfig: https://github.com/hyperrealm/libconfig

``` shell
$ make prefix=/usr sysconfdir=/etc/gpiod defconfig
$ make
$ sudo make install
```

## rules

The rules files must be placed inside the $SYSCONFDIR/rules.d directory.
"gpiod" is able to read each file and to load up to 64 gpio rules.
The rule defines the gpiochip driver and the line offset by different ways:

 - the path to the gpiochip entry
 - the name of the gpiochip if it is available from the device-tree
 - the line offset of the GPIO
 - the name of the GPIO if it is available from the device-tree

The event handler is a script must contain the path to the script and may contain arguments:

``` config
rules=({
	device = "/dev/gpiochip0";
	line = 26;
	exec = "/sbin/halt";
	},{
	chipname = "pinctrl-bcm2835";
	name = "rfkill";
	exec = "/etc/init.d/S40wifi";
	});
```

The script is called with environment variables to manage events and several GPIO inside the script:

 * GPIO the gpio line
 * CHIP a chip id
 * NAME the gpio name
 * ACTION the event "rising" or "falling"

``` shell
#!/bin/sh

echo "$CHIP $GPIO $NAME $ACTION" >> /tmp/test.log
```

The rules allow to set a script on one event type ("rising" or "falling"):

``` config
rules=({
	chipname = "pinctrl-bcm2835";
	line = 17;
	action = "rising";
	exec = "/usr/sbin/rfkill unblock wlan";
	},{
	chipname = "pinctrl-bcm2835";
	line = 17;
	action = "falling";
	exec = "/usr/sbin/rfkill block wlan";
	});
```

And it is possible to attach a specific handler to set another GPIO:

``` config
rules=({
	chipname = "pinctrl-bcm2835";
	line = 17;
	action = "falling";
	exec = "/sbin/halt";
	},{
	chipname = "pinctrl-bcm2835";
	line = 17;
	led = 27;
	});
```
