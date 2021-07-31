#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=musl-obstack
PKG_VERSION:=1.2.2
PKG_RELEASE:=$(AUTORELEASE)

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/void-linux/musl-obstack/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=6eac88961c58a53f31cccd719b97d1b33c65436c51664e760a9582e2659c1314

PKG_MAINTAINER:=
PKG_LICENSE:=GPL-2.0-or-later
PKG_LICENSE_FILES:=LICENSE

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/musl-obstack
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=musl-obstack
  URL:=https://github.com/void-linux/musl-obstack
endef

define Package/musl-obstack/description
  The musl-obstack package is a copy + paste of the obstack functions and macros found in GNU gcc libiberty library for use with musl libc.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/obstack.h $(1)/usr/include/
endef

$(eval $(call BuildPackage,musl-obstack))
