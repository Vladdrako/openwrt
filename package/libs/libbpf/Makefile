#
# Copyright (C) 2020-2024 Tony Ambardar <itugrok@yahoo.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libbpf
PKG_VERSION:=1.5.0
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/libbpf/libbpf
PKG_MIRROR_HASH:=7699fcfa89bf0f6e756c4555fbd3fdb8cbcfcc770e5944aebdd0e9244031f022
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=v$(PKG_VERSION)
PKG_ABI_VERSION:=$(firstword $(subst .,$(space),$(PKG_VERSION)))

PKG_MAINTAINER:=Tony Ambardar <itugrok@yahoo.com>
PKG_CPE_ID:=cpe:/a:libbpf_project:libbpf

PKG_BUILD_FLAGS:=no-mips16 no-gc-sections
PKG_BUILD_PARALLEL:=1
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/libbpf
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=libbpf - eBPF helper library
  LICENSE:=LGPL-2.1 OR BSD-2-Clause
  ABI_VERSION:=$(PKG_ABI_VERSION)
  URL:=http://www.kernel.org
  DEPENDS:=+libelf
endef

define Package/libbpf/description
  libbpf is a library for loading eBPF programs and reading and manipulating eBPF objects from user-space.
endef

TARGET_CFLAGS += $(FPIC)
TARGET_LDFLAGS += $(FPIC)

MAKE_FLAGS += \
	$(if $(findstring c,$(OPENWRT_VERBOSE)),V=1,V='') \
	LIBSUBDIR=lib

MAKE_PATH = src

define Build/InstallDev/libbpf
	$(INSTALL_DIR) $(1)/usr/include/bpf
	$(CP) $(PKG_INSTALL_DIR)/usr/include/bpf/*.h $(1)/usr/include/bpf/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libbpf.{a,so*} \
		$(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/libbpf.pc \
		$(1)/usr/lib/pkgconfig/
	$(SED) 's,/usr/include,$$$${prefix}/include,g' \
		$(1)/usr/lib/pkgconfig/libbpf.pc
	$(SED) 's,/usr/lib,$$$${exec_prefix}/lib,g' \
		$(1)/usr/lib/pkgconfig/libbpf.pc
endef

Build/InstallDev=$(Build/InstallDev/libbpf)

define Package/libbpf/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libbpf.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libbpf))
