# Copyright 2018-2020 Alex D (https://gitlab.com/Nooblord/)
# This is free software, licensed under the GNU General Public License v3.

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-unlocker
PKG_VERSION:=1.53
PKG_RELEASE:=17
PKG_MAINTAINER:=Alex D <alex@dreamisdead.tk>
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=Network
	TITLE:=Unlocker
	PKGARCH:=all
endef

define Package/$(PKG_NAME)/description
	ISP Restriction bypass helper tool
endef

define Package/$(PKG_NAME)/conffiles
/etc/config/unlocker
endef

define Build/Configure
endef

define Build/Compile
endef

define Build/Prepare
	$(foreach po,$(wildcard ${CURDIR}/translation/*/*.po), \
	  po2lmo $(po) $(PKG_BUILD_DIR)/$(patsubst %.po,%.lmo,$(notdir $(po)));)
	$(CP) -r ./files $(PKG_BUILD_DIR)/files
endef

define Package/$(PKG_NAME)/install
	# Copy main script and firewall
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/unlocker $(1)/etc/init.d/unlocker
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/unlocker.include $(1)/etc/unlocker.include
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/files/unlocker.conf $(1)/etc/config/unlocker

	# Copy LuCI Description and ACL
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/rootfs/usr/share/luci/menu.d/luci-app-unlocker.json \
	$(1)/usr/share/luci/menu.d/luci-app-unlocker.json
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/rootfs/usr/share/rpcd/acl.d/luci-app-unlocker.json \
	$(1)/usr/share/rpcd/acl.d/luci-app-unlocker.json

	# Copy translation
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/i18n
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/unlocker.*.lmo $(1)/usr/lib/lua/luci/i18n/

	# Copy web stuff
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/luci/controller/unlocker.lua \
	$(1)/usr/lib/lua/luci/controller/unlocker.lua
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/luci/model/cbi/unlocker.lua \
	$(1)/usr/lib/lua/luci/model/cbi/unlocker.lua
endef



define Package/$(PKG_NAME)/postinst
	#!/bin/sh
	if [ -z "$${IPKG_INSTROOT}" ]; then
	uci -q batch <<-EOF >/dev/null
		delete firewall.unlocker
		set firewall.unlocker=include
		set firewall.unlocker.type=script
		set firewall.unlocker.path=/etc/unlocker.include
		set firewall.unlocker.reload=1
		commit firewall
	EOF

	/etc/init.d/unlocker stop
	/etc/init.d/unlocker disable

	rm -f /tmp/luci-indexcache 2>/dev/null
	fi
	exit 0
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
