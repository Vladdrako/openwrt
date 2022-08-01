# 
# Copyright (C) 2006-2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=autoconf
PKG_VERSION:=2.71

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.xz
PKG_SOURCE_URL:=@GNU/autoconf
PKG_HASH:=f14c83cfebcc9427f2c3cea7258bd90df972d92eb26752da4ddad81c87a0faa4

include $(INCLUDE_DIR)/host-build.mk

HOST_CONFIGURE_ARGS += \
	--datarootdir=$(STAGING_DIR_HOST)/share

HOST_CONFIGURE_VARS += \
	EMACS="no" \
	PERL="/usr/bin/env perl"

# hack to disable building of man pages
HOST_CONFIGURE_VARS += HELP2MAN="echo"
Hooks/HostCompile/Pre += Host/Compile/Man
define Host/Compile/Man
	mkdir -p $(HOST_BUILD_DIR)/man; \
	for manpage in \
		autoconf \
		autoheader \
		autom4te \
		autoreconf \
		autoscan \
		autoupdate \
		ifnames \
		; do \
			touch $(HOST_BUILD_DIR)/man/$$$$manpage.1.t $(HOST_BUILD_DIR)/man/$$$$manpage.1; \
	done;
endef

define Host/Compile
	export SHELL="$(BASH)"; $(MAKE) -C $(HOST_BUILD_DIR)
endef

define Host/Install
	export SHELL="$(BASH)"; $(MAKE) -C $(HOST_BUILD_DIR) install
endef

define Host/Clean
	-export SHELL="$(BASH)"; $(MAKE) -C $(HOST_BUILD_DIR) uninstall
	$(call Host/Clean/Default)
endef

$(eval $(call HostBuild))
