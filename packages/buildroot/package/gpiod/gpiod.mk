################################################################################
#
# gpiod
#
################################################################################

GPIOD_VERSION = v1.0
GPIOD_SITE = $(call github,mchalain,gpiod,$(GPIOD_VERSION))

GPIOD_MAKE_OPTS+=prefix=/usr
GPIOD_MAKE_OPTS+=sysconfdir=/etc/gpiod
#GPIOD_MAKE_OPTS+=DEBUG=y

GPIOD_KCONFIG_FILE=$(GPIOD_PKGDIR)/gpiod_defconfig
GPIOD_KCONFIG_EDITORS = config
GPIOD_KCONFIG_OPTS = $(GPIOD_MAKE_OPTS)

GPIOD_DEPENDENCIES += libgpiod
GPIOD_DEPENDENCIES += libconfig

define GPIOD_LIBCONFIG_OPTS
	$(call KCONFIG_ENABLE_OPT,LIBCONFIG,$(@D)/.config)
endef

define GPIOD_KCONFIG_FIXUP_CMDS
	$(GPIOD_LIBCONFIG_OPTS)
endef

define GPIOD_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(TARGET_MAKE_ENV) \
		$(MAKE1) -C $(@D) $(GPIOD_MAKE_OPTS)
endef

define GPIOD_INSTALL_TARGET_CMDS
	$(INSTALL) -d -m 755 $(TARGET_DIR)/etc/gpiod/rules.d
	$(MAKE) -C $(@D) $(GPIOD_MAKE_OPTS) \
		DESTDIR="$(TARGET_DIR)" DEVINSTALL=n install
endef

define GPIOD_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 644 $(GPIOD_PKGDIR)/gpiod.service \
		$(TARGET_DIR)/usr/lib/systemd/system/gpiod.service
	mkdir -p $(TARGET_DIR)/etc/systemd/system/multi-user.target.wants
	ln -fs ../../../../usr/lib/systemd/system/gpiod.service \
		$(TARGET_DIR)/etc/systemd/system/multi-user.target.wants/gpiod.service
endef
define GPIOD_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 755 $(GPIOD_PKGDIR)/S20gpiod \
		$(TARGET_DIR)/etc/init.d/S20gpiod
endef

$(eval $(kconfig-package))
