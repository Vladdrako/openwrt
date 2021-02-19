#
# Copyright (C) 2016-2019 Jason A. Donenfeld <Jason@zx2c4.com>
# Copyright (C) 2016 Baptiste Jonglez <openwrt@bitsofnetworks.org>
# Copyright (C) 2016-2017 Dan Luedtke <mail@danrl.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wireguard-oot

PKG_VERSION:=1.0.20201112
PKG_RELEASE:=2

PKG_SOURCE:=wireguard-linux-compat-$(PKG_VERSION).tar.xz
PKG_SOURCE_URL:=https://git.zx2c4.com/wireguard-linux-compat/snapshot/
PKG_HASH:=89eae7f0c0bd6c8df3ba2e090984974ff68741a9f26aa0922890f8ca727897e1

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=COPYING

PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/wireguard-linux-compat-$(PKG_VERSION)
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

PKG_MAINTAINER:=Jason A. Donenfeld <Jason@zx2c4.com>

# WireGuard's makefile needs this to know where to build the kernel module
export KERNELDIR:=$(LINUX_DIR)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/wireguard/Defaults
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=WireGuard kernel module
  URL:=https://www.wireguard.com
endef

define KernelPackage/wireguard
  $(call KernelPackage/wireguard/Defaults)
  DEPENDS:=+!LINUX_5_4:kmod-wireguard-it +LINUX_5_4:kmod-wireguard-oot
endef

define KernelPackage/wireguard/description
  WireGuard is a novel VPN that runs inside the Linux Kernel and utilizes
  state-of-the-art cryptography. It aims to be faster, simpler, leaner, and
  more useful than IPSec, while avoiding the massive headache. It intends to
  be considerably more performant than OpenVPN.  WireGuard is designed as a
  general purpose VPN for running on embedded interfaces and super computers
  alike, fit for many different circumstances. It uses UDP.

  This package provides the kernel module for WireGuard.
endef

define KernelPackage/wireguard-oot
  $(call KernelPackage/wireguard/Defaults)
  HIDDEN:=1
  DEPENDS:=+IPV6:kmod-udptunnel6 +kmod-udptunnel4 @LINUX_5_4
  FILES:= $(PKG_BUILD_DIR)/src/wireguard.$(LINUX_KMOD_SUFFIX)
  AUTOLOAD:=$(call AutoProbe,wireguard)
endef

define KernelPackage/wireguard-oot/description
  $(call KernelPackage/wireguard/description)
endef

include $(INCLUDE_DIR)/kernel-defaults.mk

define Build/Compile
	$(if $(CONFIG_LINUX_5_4),$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)/src" modules)
endef

$(eval $(call KernelPackage,wireguard-oot))
$(eval $(call KernelPackage,wireguard))
