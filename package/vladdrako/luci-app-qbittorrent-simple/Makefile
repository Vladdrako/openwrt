# Copyright (C) 2019 Openwrt.org
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-qbittorrent-simple
PKG_VERSION:=1.0.0
PKG_RELEASE:=3

LUCI_TITLE:=LuCI support for qBittorrent
LUCI_PKGARCH:=all
LUCI_DEPENDS:=+qbittorrent

define Package/$(PKG_NAME)/config
	choice
		prompt "Build Version Selection"
		default PACKAGE_luci-app-qbittorrent-simple_dynamic

		config PACKAGE_luci-app-qbittorrent-simple_static
			bool "Static Build"
			depends on arm||aarch64||x86_64

		config PACKAGE_luci-app-qbittorrent-simple_dynamic
			bool "Dynamic Build"
			
	endchoice
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
