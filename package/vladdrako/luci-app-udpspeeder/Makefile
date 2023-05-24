#
# Copyright (C) 2018 Hao Dong <halbertdong@gmail.com>
#
# This is free software, licensed under the GNU General Public License v3.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-udpspeeder
PKG_VERSION:=1.1.0
PKG_RELEASE:=2

PKG_LICENSE:=GPLv3
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Hao Dong <halbertdong@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=luci
	CATEGORY:=LuCI
	SUBMENU:=3. Applications
	TITLE:=LuCI Support for UDPspeeder
	PKGARCH:=all
	DEPENDS:=+UDPspeeder
endef

define Package/$(PKG_NAME)/description
	LuCI Support for UDPspeeder.
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	( . /etc/uci-defaults/luci-udpspeeder ) && rm -f /etc/uci-defaults/luci-udpspeeder
fi
exit 0
endef

define Package/$(PKG_NAME)/conffiles
	/etc/config/udpspeeder
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) ./files/luci/controller/*.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/udpspeeder
	$(INSTALL_DATA) ./files/luci/model/cbi/udpspeeder/*.lua $(1)/usr/lib/lua/luci/model/cbi/udpspeeder/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/udpspeeder
	$(INSTALL_DATA) ./files/luci/view/udpspeeder/*.htm $(1)/usr/lib/lua/luci/view/udpspeeder/
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/root/etc/uci-defaults/luci-udpspeeder $(1)/etc/uci-defaults/luci-udpspeeder
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
