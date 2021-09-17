#
# Copyright (C) 2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=gettext
PKG_RELEASE:=3

PKG_MAINTAINER:=Jo-Philipp Wich <jo@mein.io>
PKG_LICENSE:=FSFULLR
PKG_LICENSE_FILES:=LICENSE

PKG_HOST_ONLY:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk

define Package/libintl
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Stub header for the GNU Internationalization library
  BUILDONLY:=1
endef

define Host/Prepare
	mkdir -p $(HOST_BUILD_DIR)
endef

define Host/Compile
endef

define Host/Install
	$(INSTALL_DIR) $(STAGING_DIR_HOSTPKG)/include
	$(INSTALL_DATA) ./src/include/libintl.h $(STAGING_DIR_HOSTPKG)/include/

	$(INSTALL_DIR) $(STAGING_DIR_HOSTPKG)/share/aclocal
	$(INSTALL_DATA) ./src/m4/* $(STAGING_DIR_HOSTPKG)/share/aclocal/
endef

$(eval $(call HostBuild))
$(eval $(call BuildPackage,libintl))
