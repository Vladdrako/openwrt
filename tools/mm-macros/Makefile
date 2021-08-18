#
# Copyright (C) 2010-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mm-macros
PKG_VERSION:=1.0.3

PKG_SOURCE_URL:=@GNOME/mm-common/1.0
PKG_SOURCE:=mm-common-$(PKG_VERSION).tar.xz
PKG_HASH:=e81596625899aacf1d0bf27ccc2fcc7f373405ec48735ca1c7273c0fbcdc1ef5

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/mm-common-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/meson.mk

define Host/Install
	$(INSTALL_DIR) $(STAGING_DIR_HOST)/share/aclocal
	$(INSTALL_DATA) $(HOST_BUILD_DIR)/macros/*.m4 $(STAGING_DIR_HOST)/share/aclocal/
endef

$(eval $(call HostBuild))
