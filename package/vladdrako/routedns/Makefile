# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2022 openwrt.org

include $(TOPDIR)/rules.mk

PKG_NAME:=routedns
PKG_VERSION:=20221122
PKG_RELEASE:=$(AUTORELEASE)

PKG_SOURCE_URL:=https://github.com/folbricht/routedns.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-09-03
PKG_SOURCE_VERSION:=88324f88c02111f69c502ae5eb1230a86ff50da9
PKG_MIRROR_HASH:=

PKG_MAINTAINER:=Vladdrako <vladdrako007@gmail.com>
PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=github.com/folbricht/routedns
GO_PKG_BUILD_VARS+= on go get -v github.com/folbricht/routedns/cmd/routedns

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/golang/golang-package.mk

define Package/routedns
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=IP Addresses and Names
  TITLE:=DNS stub resolver, proxy and router with support for DoT, DoH, DoQ, and DTLS
  URL:=https://github.com/folbricht/routedns
  DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle
  USERID:=routedns=412:routedns=412
endef

define Package/routedns/description
  RouteDNS acts as a stub resolver and proxy that offers flexible configuration options with a focus on providing privacy as well as resiliency. It supports several DNS protocols such as plain UDP and TCP, DNS-over-TLS and DNS-over-HTTPS as input and output. In addition it's possible to build complex processing pipelines allowing routing of queries based on query name, type or source address as well as blocklists, caches and name translation. Upstream resolvers can be grouped in various ways to provide failover, load-balancing, or performance.
endef

define Package/routedns/install
	$(call GoPackage/Package/Install/Bin,$(1))

	$(INSTALL_DIR) $(1)/etc/config/
	$(INSTALL_CONF) $(CURDIR)/files/routedns.config $(1)/etc/config/routedns
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) $(CURDIR)/files/routedns.init $(1)/etc/init.d/routedns
endef

#define Package/routedns/conffiles
#/etc/config/routedns
#endef

$(eval $(call GoBinPackage,routedns))
$(eval $(call BuildPackage,routedns))
 
