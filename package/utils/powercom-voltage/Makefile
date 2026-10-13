include $(TOPDIR)/rules.mk

PKG_NAME:=powercom-voltage
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Vladdrako
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/powercom-voltage
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Powercom RPT-800AP Voltage Reader
  DEPENDS:=+libusb-1.0
endef

define Package/powercom-voltage/description
  A small utility to read battery voltage from Powercom RPT-800AP UPS using libusb.
  Required because NUT driver fails to read non-standard ASCII reports (Report 0xa4).
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/powercom_voltage \
		$(PKG_BUILD_DIR)/powercom_voltage.c $(TARGET_LDFLAGS) -lusb-1.0
endef

define Package/powercom-voltage/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/powercom_voltage $(1)/usr/bin/
endef

$(eval $(call BuildPackage,powercom-voltage))
