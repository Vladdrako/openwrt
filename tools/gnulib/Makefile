#
# Copyright (C) 2009-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=gnulib
PKG_VERSION:=2023-01-11
PKG_SOURCE_VERSION:=e538b158cf08cc3632dc646468b58b7248d055a7

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.xz
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://git.savannah.gnu.org/git/gnulib.git
PKG_MIRROR_HASH:=c1997de438d64d69069b752f02299c37f7bf1122f9371ef54878391667907fe1
PKG_HASH:=c1997de438d64d69069b752f02299c37f7bf1122f9371ef54878391667907fe1

include $(INCLUDE_DIR)/host-build.mk

define Host/Configure
	:
endef

define Host/Compile
	:
endef

define Host/Install
	rm -rf $(STAGING_DIR_HOST)/share/gnulib
	$(CP) $(HOST_BUILD_DIR)/. $(STAGING_DIR_HOST)/share/gnulib
	ln -sf ../share/gnulib/gnulib-tool $(STAGING_DIR_HOST)/bin/gnulib-tool
endef

define Host/Uninstall
	rm -rf $(STAGING_DIR_HOST)/bin/gnulib-tool $(STAGING_DIR_HOST)/share/gnulib
endef

$(eval $(call HostBuild))
