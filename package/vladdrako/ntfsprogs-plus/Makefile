#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ntfsprogs-plus
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/ntfsprogs-plus/ntfsprogs-plus
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2025-10-16
PKG_SOURCE_VERSION:=9cd989100e33623b0acd44c2edc14b6c8c89229b

PKG_LICENSE:=GPL-2.0-only LGPL-2.1-or-later

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

# define build dir, respect fuseext/int
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/ntfsprogs-plus
  SECTION:=utils
  CATEGORY:=Utilities
  URL:=https://www.tuxera.com
  SUBMENU:=Filesystem
  TITLE:=New Read/Write NTFSPLUS Driver
  DEPENDS:=+libuuid
endef

define Package/ntfsprogs-plus/description
  ntfsprogs-plus project focus on filesystem utilities based on ntfs-3g project to support kernel ntfs filesystem. ntfsprogs-plus takes some utilities from ntfs-3g only what it need. (ntfs-3g consist of several useful utilities and user-level   filesystem code using FUSE)

  ntfs-3g was the only solution to support ntfs for free in linux. They also support several tools to manage and debug ntfs filesystem like ntfsclone, ntfsinfo, ntfscluster. Those utilities are very useful, but ntfs-3g does not support filesystem check utility. They just support ntfsfix which is a utility that only fixes boot sector with mirror boot sector, and a rare bug case of Windows XP (called by self-located MFT), reset journal.

  So, ntfsprogs-plus try to implement checking filesystem utility which is named ntfsck. You may think ntfsck is a linux version of chkdsk of Windows. ntfsprogs-plus use a little modified ntfs-3g library for fsck. ntfs-3g have some memory bugs   and restriction. ntfsprogs-plus also try to remove memory bug and restriction.

  At first release, ntfsck fully check filesystem and repair it. And not yet support journal replay.

endef

#CONFIGURE_ARGS += \
#	--enable-shared \
#	--enable-static \
#	--with-uuid

# redefine prepare to extract to our build dir
# apply patches
#define Build/Prepare
#	rm -rf $(PKG_BUILD_DIR)/
#	mkdir -p $(PKG_BUILD_DIR)/
#	$(TAR) -xzf $(DL_DIR)/$(PKG_SOURCE) -C $(PKG_BUILD_DIR) --strip 1
#	$(Build/Patch)
#endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP)	$(PKG_INSTALL_DIR)/usr/include $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP)	$(PKG_INSTALL_DIR)/usr/lib/libntfs.{la,a,so*} $(1)/usr/lib/
endef

define Package/ntfsprogs-plus/install
	rm -f $(PKG_INSTALL_DIR)/usr/sbin/*.ntfs*
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/* $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libntfs.so.* $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/* $(1)/usr/sbin/
	$(LN) mkntfs $(1)/usr/sbin/mkfs.ntfs
	$(LN) ntfsck $(1)/usr/sbin/fsck.ntfs
endef
#$(LN) ../usr/bin/ntfs-3g $(1)/sbin/mount.ntfs-3g
#define Package/ntfsprogs-plus/postinst
#!/bin/sh
#FILE="$${IPKG_INSTROOT}/etc/filesystems"
#ID="ntfs-3g"

#if ! [ -f '/etc/filesystems' ]; then
#	echo "Create '$$FILE'."
#	touch "$$FILE"
#fi

#if ! grep -q -e '^ntfs-3g$$' "$$FILE"; then
#	echo "Add '$$ID' to known filesystems."
#	echo "$$ID" >> "$$FILE"
#fi

#endef

#define Package/ntfs-3g-low/install
#	$(INSTALL_DIR) $(1)/usr/bin
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/lowntfs-3g $(1)/usr/bin/
#	$(INSTALL_DIR) $(1)/sbin
#	$(LN) ../usr/bin/lowntfs-3g $(1)/sbin/mount.lowntfs-3g
#endef

#define Package/ntfs-3g-low/postinst
#!/bin/sh
#FILE="$${IPKG_INSTROOT}/etc/filesystems"
#ID="lowntfs-3g"

#if ! [ -f '/etc/filesystems' ]; then
#	echo "Create '$$FILE'."
#	touch "$$FILE"
#fi

#if ! grep -q -e '^ntfs-3g$$' "$$FILE"; then
#	echo "Add '$$ID' to known filesystems."
#	echo "$$ID" >> "$$FILE"
#fi

#endef

#define Package/ntfsprogs-plus/install
#	rm -f $(PKG_INSTALL_DIR)/usr/sbin/*.ntfs*
#	$(INSTALL_DIR) $(1)/usr/{bin,sbin}
#	$(FIND) $(PKG_INSTALL_DIR)/usr/bin/ -type f ! -regex '.*[^/]*ntfs-3g[^/]*' -exec $(INSTALL_BIN) {} $(1)/usr/bin/ \;
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/* $(1)/usr/sbin/
#	$(LN) mkntfs $(1)/usr/sbin/mkfs.ntfs
#endef

$(eval $(call BuildPackage,ntfsprogs-plus))
