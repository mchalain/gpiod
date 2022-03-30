################################################################################
#
# gpiod
#
################################################################################

GPIOD_VERSION = 1.1
GPIOD_SITE = $(call github,mchalain,gpiod,$(GPIOD_VERSION))
GPIOD_LICENSE = BSD
GPIOD_LICENSE_FILES = LICENSE

PREFIX=/usr
SYSCONFDIR=/etc/gpiod

GPIOD_MAKE_OPTS = \
	prefix=$(PREFIX) \
	sysconfdir=$(SYSCONFDIR)

#GPIOD_MAKE_OPTS+=DEBUG=y
#GPIOD_MAKE_OPTS+=V=1

GPIOD_DEPENDENCIES = \
	libgpiod \
	libconfig

define GPIOD_INSTALL_EXAMPLES
	$(INSTALL) -D -m 644 $(@D)/rules.d/10_rfkill.conf $(TARGET_DIR)/etc/gpiod/rules.d/10_rfkill.conf
	$(INSTALL) -D -m 644 $(@D)/rules.d/10_poweroff.conf $(TARGET_DIR)/etc/gpiod/rules.d/10_poweroff.conf
endef

define GPIOD_CONFIGURE_CMDS
	$(TARGET_CONFIGURE_OPTS) $(TARGET_MAKE_ENV) \
		$(MAKE1) -C $(@D) $(GPIOD_MAKE_OPTS) defconfig
endef

define GPIOD_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(TARGET_MAKE_ENV) \
		$(MAKE1) -C $(@D) $(GPIOD_MAKE_OPTS)
endef

define GPIOD_INSTALL_TARGET_CMDS
	$(INSTALL) -d -m 755 $(TARGET_DIR)$(SYSCONFDIR)/rules.d
	$(MAKE) -C $(@D) $(GPIOD_MAKE_OPTS) \
		DESTDIR="$(TARGET_DIR)" install
endef

define GPIOD_INSTALL_INIT_SYSTEMD
	cp $(GPIOD_PKGDIR)/gpiod.service.in $(@D)/gpiod.service
	$(SED) "s,@PREFIX@,$(PREFIX),g" $(@D)/gpiod.service
	$(INSTALL) -D -m 644 $(@D)/gpiod.service \
		$(TARGET_DIR)/usr/lib/systemd/system/gpiod.service
endef
define GPIOD_INSTALL_INIT_SYSV
	cp $(GPIOD_PKGDIR)/S20gpiod.in $(@D)/S20gpiod
	$(SED) "s,@PREFIX@,$(PREFIX),g" $(@D)/S20gpiod
	$(INSTALL) -D -m 755 $(@D)/S20gpiod \
		$(TARGET_DIR)/etc/init.d/S20gpiod
endef

ifeq ($(BR2_PACKAGE_GPIOD_EXAMPLES),y)
GPIOD_POST_INSTALL_TARGET_HOOKS += GPIOD_INSTALL_EXAMPLES
endif

$(eval $(generic-package))
