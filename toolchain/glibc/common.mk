#
# Copyright (C) 2006-2020 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=glibc
PKG_VERSION:=2.41
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=6e489c17f827317bcf8544efefa65f13b5a079dc
PKG_MIRROR_HASH:=37527af9a3cbc41201b8a8bead3e6ec973e922e11639ff1762de50c46cf4913e
PKG_SOURCE_URL:=https://sourceware.org/git/glibc.git
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.zst
PKG_CPE_ID:=cpe:/a:gnu:glibc

HOST_BUILD_DIR:=$(BUILD_DIR_TOOLCHAIN)/$(PKG_SOURCE_SUBDIR)
CUR_BUILD_DIR:=$(HOST_BUILD_DIR)-$(VARIANT)
PATCH_DIR:=$(PATH_PREFIX)/patches

include $(INCLUDE_DIR)/toolchain-build.mk

HOST_STAMP_PREPARED:=$(HOST_BUILD_DIR)/.prepared
HOST_STAMP_CONFIGURED:=$(CUR_BUILD_DIR)/.configured
HOST_STAMP_BUILT:=$(CUR_BUILD_DIR)/.built
HOST_STAMP_INSTALLED:=$(TOOLCHAIN_DIR)/stamp/.glibc_$(VARIANT)_installed

ifeq ($(ARCH),mips64)
  ifdef CONFIG_MIPS64_ABI_N64
    TARGET_CFLAGS += -mabi=64
  endif
  ifdef CONFIG_MIPS64_ABI_N32
    TARGET_CFLAGS += -mabi=n32
  endif
  ifdef CONFIG_MIPS64_ABI_O32
    TARGET_CFLAGS += -mabi=32
  endif
endif

ifeq ($(CONFIG_GLIBC_TUNE_MALLOC_ARENA),y)
  TARGET_CFLAGS += \
    -DOPENWRT_TUNE_MALLOC_ARENA=1 \
    -DOPENWRT_MALLOC_ARENA_PERCORE_MULT=$(CONFIG_GLIBC_MALLOC_ARENA_PERCORE_MULT) \
    -DOPENWRT_MALLOC_ARENA_CORE_LIMIT=$(CONFIG_GLIBC_MALLOC_ARENA_CORE_LIMIT)
endif

# -Os miscompiles w. 2.24 gcc5/gcc6
# only -O2 tested by upstream changeset
# "Optimize i386 syscall inlining for GCC 5"
GLIBC_CONFIGURE:= \
	unset LD_LIBRARY_PATH; \
	BUILD_CC="$(HOSTCC)" \
	$(TARGET_CONFIGURE_OPTS) \
	CFLAGS="-O2 $(filter-out -Os,$(call qstrip,$(TARGET_CFLAGS)))" \
	libc_cv_slibdir="/lib" \
	use_ldconfig=no \
	$(HOST_BUILD_DIR)/$(GLIBC_PATH)configure \
		--prefix= \
		--build=$(GNU_HOST_NAME) \
		--host=$(REAL_GNU_TARGET_NAME) \
		--with-headers=$(TOOLCHAIN_DIR)/include \
		--disable-profile \
		--disable-werror \
		--without-gd \
		--without-cvs \
		--enable-add-ons \
		--$(if $(CONFIG_SOFT_FLOAT),without,with)-fp \
		  $(if $(CONFIG_PKG_CC_STACKPROTECTOR_REGULAR),--enable-stack-protector=yes) \
		  $(if $(CONFIG_PKG_CC_STACKPROTECTOR_STRONG),--enable-stack-protector=strong) \
		  $(if $(CONFIG_PKG_CC_STACKPROTECTOR_ALL),--enable-stack-protector=all) \
		  $(if $(CONFIG_PKG_RELRO_FULL),--enable-bind-now) \
		  $(if $(CONFIG_PKG_FORTIFY_SOURCE_1),--enable-fortify-source=1) \
		  $(if $(CONFIG_PKG_FORTIFY_SOURCE_2),--enable-fortify-source=2) \
		--enable-kernel=6.6.0

export libc_cv_ssp=no
export libc_cv_ssp_strong=no
export ac_cv_header_cpuid_h=yes
export HOST_CFLAGS := $(HOST_CFLAGS) -idirafter $(CURDIR)/$(PATH_PREFIX)/include

define Host/SetToolchainInfo
	$(SED) 's,^\(LIBC_TYPE\)=.*,\1=$(PKG_NAME),' $(TOOLCHAIN_DIR)/info.mk
	$(SED) 's,^\(LIBC_URL\)=.*,\1=http://www.gnu.org/software/libc/,' $(TOOLCHAIN_DIR)/info.mk
	$(SED) 's,^\(LIBC_VERSION\)=.*,\1=$(PKG_VERSION),' $(TOOLCHAIN_DIR)/info.mk
	$(SED) 's,^\(LIBC_SO_VERSION\)=.*,\1=$(PKG_VERSION),' $(TOOLCHAIN_DIR)/info.mk
endef

define Host/Configure
	mkdir -p $(CUR_BUILD_DIR)
	( cd $(CUR_BUILD_DIR); rm -f config.cache; \
		$(GLIBC_CONFIGURE) \
	);
endef

define Host/Prepare
	$(call Host/Prepare/Default)
	ln -snf $(PKG_SOURCE_SUBDIR) $(BUILD_DIR_TOOLCHAIN)/$(PKG_NAME)
endef

define Host/Clean
	rm -rf $(CUR_BUILD_DIR)* \
		$(BUILD_DIR_TOOLCHAIN)/$(LIBC)-dev \
		$(BUILD_DIR_TOOLCHAIN)/$(PKG_NAME)
endef
