# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2020 Paul Spooren <mail@aparcar.org>
#
###
### size_compare - compare size of OpenWrt packages against upstream
###
### The script compares locally compiled package with the package indexes
### available upstream. This way the storage impact of optimizations or
### feature modifiactions is easy to see.
###
### If no environmental variables are set the scritp reads the current
### .config file. The evaluated env variables are the following:
###
###   TARGET SUBTARGET ARCH PACKAGES BIN_DIR BASE_URL CHECK_INSTALLED
###
### Usage:
###   ./scripts/size_compare.sh
###
### Options:
###   -p --package-size 	Check IPK package size and not installed size
###   -h --help 		This message

CONFIG_TARGET=$(sed -n 's/^CONFIG_TARGET_BOARD="\(.*\)"$/\1/p' .config)
CONFIG_SUBTARGET=$(sed -n 's/^CONFIG_TARGET_SUBTARGET="\(.*\)"$/\1/p' .config)
CONFIG_ARCH=$(sed -n 's/^CONFIG_TARGET_ARCH_PACKAGES="\(.*\)"$/\1/p' .config)
CONFIG_PACKAGES=$(sed -n 's/^CONFIG_PACKAGE_\(.*\)=y$/\1/p' .config | tr '\n' ' ')
CONFIG_BIN_DIR=$(sed -n 's/^CONFIG_BINARY_DIR="\(.*\)"$/\1/p' .config)

TARGET=${TARGET:-$CONFIG_TARGET}
SUBTARGET=${SUBTARGET:-$CONFIG_SUBTARGET}
ARCH=${ARCH:-$CONFIG_ARCH}
PACKAGES=${PACKAGES:-$CONFIG_PACKAGES}
BIN_DIR=${CONFIG_BIN_DIR:-./bin}
BASE_URL="${BASE_URL:-https://downloads.openwrt.org/snapshots}"
CHECK_INSTALLED="${CHECK_INSTALLED:-y}"

TARGET_URL="$BASE_URL/targets/$TARGET/$SUBTARGET/packages/Packages.gz"
PACKAGES_URL="$BASE_URL/packages/$ARCH/base/Packages.gz"

help() {
    sed -rn 's/^### ?//;T;p' "$0"
}

package_size () {
	FOUND_PACKAGE=
	if [ -z "$CHECK_INSTALLED" ]; then
		SEARCH_PATTERN="Size"
	else
		SEARCH_PATTERN="Installed-Size"
	fi
	while IFS= read -r line; do
		if [ "$line" = "Package: $2" ]; then
			FOUND_PACKAGE=y
		fi
		if [ -n "$FOUND_PACKAGE" ]; then
			case $line in
				"$SEARCH_PATTERN"*)
					echo "$line" | cut -d ' ' -f 2
					break
					;;
			esac
		fi
	done < "$1"
}

compare_sizes () {
	for PACKAGE in $PACKAGES; do
		if [ "$PACKAGE" = "libc" ]; then
			continue
		fi
		PACKAGE_FILE=$(find "$BIN_DIR/packages/$ARCH/" \
			"$BIN_DIR/targets/$TARGET/$SUBTARGET/" \
			-name "${PACKAGE}_*.ipk" 2>/dev/null | head -n1)

		if [ -z "$PACKAGE_FILE" ]; then
			continue
		fi
		if [ -z "$CHECK_INSTALLED" ]; then
			SIZE_LOCAL=$(stat -c '%s' "$PACKAGE_FILE")
		else
			SIZE_LOCAL=$(tar tzvf "$PACKAGE_FILE" | grep data.tar.gz | awk '{ print $3 }')
		fi
		SIZE_UPSTREAM=$(package_size "$TMP_INDEX" "$PACKAGE")
		SIZE_DIFF="$((SIZE_LOCAL-SIZE_UPSTREAM))"
		if [ "$SIZE_DIFF" -gt 0 ]; then
			SIZE_DIFF="+$SIZE_DIFF"
		fi
		if [ -n "$SIZE_UPSTREAM" ]; then
			echo "${SIZE_DIFF}	${SIZE_LOCAL}	${SIZE_UPSTREAM}	$PACKAGE"
		else
			echo "$PACKAGE is missing upstream"
		fi
	done
}

if [ "$1" = "-h" ]; then
    help
    exit 0
fi

if [ "$1" = "-p" ]; then
    CHECK_INSTALLED=
fi

echo "Compare packages of $TARGET/$SUBTARGET/$ARCH":
echo "$PACKAGES"
echo

if [ -z "$CHECK_INSTALLED" ]; then
	echo "Checking IPK package size"
else
	echo "Checking installed size"
fi
echo

echo "Fetching latest package indexes..."
TMP_INDEX=$(mktemp /tmp/size_compare_package_index.XXXXXX)
curl "$TARGET_URL" | gzip -d > "$TMP_INDEX" || exit 1
curl "$PACKAGES_URL" | gzip -d >> "$TMP_INDEX" || exit 1
echo

echo "Comparing package sizes..."
echo "Change 	Local	Remote 	Package"
compare_sizes | sort -g -r

rm "$TMP_INDEX"
