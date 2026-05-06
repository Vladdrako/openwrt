include $(TOPDIR)/rules.mk
PKG_NAME:=torrserver
PKG_VERSION:=matriX.137
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_SOURCE_VERSION).tar.gz
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/YouROK/TorrServer.git
PKG_SOURCE_VERSION:=MatriX.137
PKG_MIRROR_HASH:=skip

PKG_LICENSE:=GPL-3.0
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=Vladdrako

PKG_BUILD_DEPENDS:=golang/host upx/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=github.com/YouROK/TorrServer
GO_PKG_LDFLAGS_X:=main.version=$(PKG_SOURCE_VERSION) main.arch=$(ARCH)
GO_PKG_LDFLAGS:=-s -w

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/golang/golang-package.mk

define Package/torrserver
  SECTION:=multimedia
  CATEGORY:=Multimedia
  TITLE:=TorrServer Matrix - Torrent streaming server
  URL:=https://github.com/YouROK/TorrServer
  DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle
endef

define Package/torrserver/description
  TorrServer is a torrent streaming server that allows you to stream torrents 
  to media players via HTTP. It supports caching in RAM and disk.
endef

define Build/Compile
	( \
		cd $(PKG_BUILD_DIR)/server && \
		$(GO_PKG_VARS) \
		go build \
			-tags "$(GO_PKG_TAGS)" \
			-ldflags "$(GO_PKG_LDFLAGS)" \
			-o $(PKG_BUILD_DIR)/torrserver \
			./cmd \
	)
	$(STAGING_DIR_HOST)/bin/upx --lzma --best $(PKG_BUILD_DIR)/torrserver
endef

define Package/torrserver/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/torrserver $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/torrserver.init $(1)/etc/init.d/torrserver
endef

$(eval $(call GoBinPackage,torrserver))
$(eval $(call BuildPackage,torrserver))
