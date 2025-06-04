#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=squashfs4
PKG_CPE_ID:=cpe:/a:phillip_lougher:squashfs
PKG_VERSION:=4.7.0
PKG_RELEASE=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/plougher/squashfs-tools
PKG_SOURCE_DATE:=2025-06-04
PKG_SOURCE_VERSION:=2e87d42ed089dc31990d83eeb07437b9d085d6d1
PKG_MIRROR_HASH:=ff5c545b8d64e1c3a1316abde0b6ba297c267fa3daed264eff2038dc99649869

HOST_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/host-build.mk
HOST_BUILD_SUBDIR:=$(HOST_BUILD_DIR)/squashfs-tools
HOST_MAKE_VARS += INSTALL_PREFIX="$(HOST_BUILD_PREFIX)"
HOST_MAKE_FLAGS += -C $(HOST_BUILD_SUBDIR)
HOST_MAKE_FLAGS += XATTR_SUPPORT=1 XATTR_DEFAULT=0
HOST_MAKE_FLAGS += GZIP_SUPPORT=1
HOST_MAKE_FLAGS += LZ4_SUPPORT=1
HOST_MAKE_FLAGS += LZMA_XZ_SUPPORT=1
HOST_MAKE_FLAGS += LZO_SUPPORT=1
HOST_MAKE_FLAGS += XZ_SUPPORT=1 XZ_EXTENDED_OPTIONS=1
HOST_MAKE_FLAGS += ZSTD_SUPPORT=1
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_GZIP),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=gzip
endif
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_LZ4),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=lz4
endif
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_LZO),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=lzo
endif
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_XZ),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=xz
endif
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_ZSTD),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=zstd
endif
ifeq ($(CONFIG_KERNEL_SQUASHFS_COMP_DEFAULT_UNSPEC),y)
	HOST_MAKE_FLAGS += COMP_DEFAULT=zstd
endif

define Host/Configure
endef

define Host/Compile
	$(call Host/Compile/Default,mksquashfs)
	$(call Host/Compile/Default,unsquashfs)
endef

define Host/Install
	$(call Host/Compile/Default,install)
endef

define Host/Uninstall
	$(call Host/Compile/Default,uninstall)
	$(call Host/Compile/Default,clean)
endef

define Host/Clean
endef

$(eval $(call HostBuild))
