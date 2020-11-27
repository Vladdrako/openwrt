include $(TOPDIR)/rules.mk

PKG_NAME:=qbittorrent
PKG_VERSION:=4.3.1.10
PKG_RELEASE=0

PKG_SOURCE:=release-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/c0re100/qBittorrent-Enhanced-Edition/archive
PKG_BUILD_DIR:=$(BUILD_DIR)/qBittorrent-Enhanced-Edition-release-$(PKG_VERSION)
PKG_HASH:=569baa36d68a9fbe4938da80e5d73f9efe4b08e31b83d6ca25b943fae599bef8

PKG_LICENSE:=GPL-2.0+
PKG_LICENSE_FILES:=COPYING

PKG_BUILD_PARALLEL:=1
PKG_INSTALL:=1
PKG_USE_MIPS16:=0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/qbittorrent
	SECTION:=net
	CATEGORY:=Network
	SUBMENU:=BitTorrent
	DEPENDS:=+qt5-network +qt5-sql +qt5-xml +rblibtorrent $(ICONV_DEPENDS) $(INTL_DEPENDS)
	TITLE:=bittorrent client programmed in C++ / Qt
	URL:=https://www.qbittorrent.org/
	PROVIDES:=qBittorrent
endef

define Package/qbittorrent/description
 qBittorrent (Enhanced Edition) is a bittorrent client programmed in C++ / Qt that uses
 libtorrent (sometimes called libtorrent-rasterbar) by Arvid Norberg.
 It aims to be a good alternative to all other bittorrent clients out
 there. qBittorrent is fast, stable and provides unicode support as
 well as many features.
endef

TARGET_LDFLAGS += $(if $(INTL_FULL),-liconv) $(if $(INTL_FULL),-lintl)

CONFIGURE_VARS += \
	QT_QMAKE="$(TOOLCHAIN_DIR)/bin" \
	PKG_CONFIG_PATH="$(TOOLCHAIN_DIR)/lib/pkgconfig"

CONFIGURE_ARGS += \
	--disable-gui \
	--with-boost=$(STAGING_DIR)/usr

MAKE_VARS += \
	INSTALL_ROOT="$(PKG_INSTALL_DIR)"

define Package/qbittorrent/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/qbittorrent-nox $(1)/usr/bin
endef

$(eval $(call BuildPackage,qbittorrent))
