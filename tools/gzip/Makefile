#
# Copyright (C) 2022 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gzip
PKG_VERSION:=1.12

# https://ftp.gnu.org/gnu/gzip/gzip-1.12.tar.xz
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.xz
PKG_SOURCE_URL:=https://ftp.gnu.org/gnu/gzip/
PKG_HASH:=ce5e03e519f637e1f814011ace35c4f87b33c0bbabeec35baf5fbd3479e91956

PKG_LICENSE:=GPL
PKG_LICENSE_FILES:=README
PKG_CPE_ID:=cpe:/a:gnu:gzip

HOST_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/host-build.mk

HOSTCC := $(HOSTCC_NOCACHE)
HOST_CFLAGS += $(HOST_FPIC)

HOST_CONFIGURE_ARGS = \
	--prefix=$(HOST_BUILD_PREFIX) \
	--sysconfdir=$(HOST_BUILD_PREFIX)/etc \
	--localstatedir=$(HOST_BUILD_PREFIX)/var \
	--libdir=$(HOST_BUILD_PREFIX)/lib \
	--includedir=$(HOST_BUILD_PREFIX)/include

define Host/Uninstall
	-$(call Host/Compile/Default,uninstall)
endef

$(eval $(call HostBuild))
