include $(TOPDIR)/rules.mk

PKG_NAME:=smartdns-rs
PKG_VERSION:=0.13.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mokeyish/smartdns-rs.git
PKG_SOURCE_VERSION:=v$(PKG_VERSION)
PKG_MIRROR_HASH:=skip

PKG_LICENSE:=GPL-3.0-only
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DEPENDS:=rust/host
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/rust/rust-package.mk

define Package/smartdns-rs
  SECTION:=net
  CATEGORY:=Network
  TITLE:=SmartDNS RS
  URL:=https://github.com/mokeyish/smartdns-rs
  DEPENDS:=$(RUST_ARCH_DEPENDS) +libopenssl +ca-bundle
endef

define Package/smartdns-rs/description
  A cross platform local DNS server written in rust.
endef

define Package/smartdns-rs/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/bin/smartdns $(1)/usr/sbin/smartdns-rs
	
	$(INSTALL_DIR) $(1)/etc/smartdns
	$(INSTALL_CONF) ./files/etc/smartdns/smartdns.conf $(1)/etc/smartdns/smartdns.conf
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/smartdns $(1)/etc/init.d/smartdns
endef

$(eval $(call RustBinPackage,smartdns-rs))
$(eval $(call BuildPackage,smartdns-rs))
