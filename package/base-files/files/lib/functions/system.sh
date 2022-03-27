# Copyright (C) 2006-2013 OpenWrt.org

. /lib/functions.sh

get_mac_binary() {
	local path="$1"
	local offset="${2:-0}"
	local length="${3:-6}"
	local mac

	if ! [ -e "$path" ]; then
		echo "get_mac_binary: file $path not found!" >&2
		return
	fi

	mac=$(head -c $((offset + length)) "$path" | tail -c "$length" |
		hexdump -e '6/1 "%02x"' 2>/dev/null)

	macaddr_canonicalize "$mac"
}

get_mac_label_dt() {
	local basepath="/proc/device-tree"
	local macdevice macaddr

	macdevice=$(cat "$basepath/aliases/label-mac-device" 2>/dev/null)
	[ -n "$macdevice" ] || return

	[ -e "$basepath/$macdevice/mac-address" ] &&
		macaddr=$(get_mac_binary "$basepath/$macdevice/mac-address" 2>/dev/null) ||
		macaddr=$(get_mac_binary "$basepath/$macdevice/local-mac-address" 2>/dev/null)

	echo -n "$macaddr"
}

get_mac_label_json() {
	local cfg="/etc/board.json"
	local macaddr

	[ -s "$cfg" ] || return

	. /usr/share/libubox/jshn.sh

	json_init
	json_load "$(cat $cfg)"
	if json_is_a system object; then
		json_select system
			json_get_var macaddr label_macaddr
		json_select ..
	fi

	echo -n "$macaddr"
}

get_mac_label() {
	local macaddr

	[ -n "$(get_mac_label_dt)" ] &&
		macaddr=$(get_mac_label_dt) ||
		macaddr=$(get_mac_label_json)

	macaddr_canonicalize "$macaddr"
}

find_mtd_chardev() {
	local mtdname="$1"
	local PREFIX INDEX

	INDEX=$(find_mtd_index "$mtdname")
	[ -d /dev/mtd ] && PREFIX="/dev/mtd/" || PREFIX="/dev/mtd"

	echo -n "${INDEX:+$PREFIX$INDEX}"
}

mtd_get_mac_ascii() {
	local mtdname="$1"
	local key="$2"
	local part value prefix suffix

	part=$(find_mtd_part "$mtdname")
	if [ -z "$part" ]; then
		echo "mtd_get_mac_ascii: partition $mtdname not found!" >&2
		return
	fi

	value=$(strings "$part" |
		while read a; do
			[[ "$a" == *"$key"* ]] && echo -n "$a" && break
		done)

	value="${value#*${key}}"
	value="${value//$operators/}"

	# if quoted
	[[ "$value" != "${value//$blockchar/}" ]] &&
		value="${value#*${blockchar}}" && value="${value%%${blockchar}*}"

	prefix="${value%%${delimiter}*}"
	suffix="${value#*${delimiter}}"

	# if no delimiter
	[[ "$prefix" == "$suffix" ]] &&
		prefix="${suffix:0:2}" && suffix="${suffix:2:10}"

	macaddr_canonicalize "${prefix:(-2)}${suffix//$delimiter/}"
}

mtd_get_mac_text() {
	local mtdname="$1"
	local offset=$(($2))
	local length="${3:-17}"
	local part mac

	part=$(find_mtd_part "$mtdname")
	if [ -z "$part" ]; then
		echo "mtd_get_mac_text: partition $mtdname not found!" >&2
		return
	fi

	if [ -z "$offset" ]; then
		echo "mtd_get_mac_text: offset missing!" >&2
		return
	fi

	mac=$(head -c $((offset + length)) "$part" | tail -c "$length")

	macaddr_canonicalize "$mac"
}

mtd_get_mac_binary() {
	local mtdname="$1"
	local offset="$2"
	local part

	part=$(find_mtd_part "$mtdname")
	get_mac_binary "$part" "$offset"
}

mtd_get_mac_binary_ubi() {
	local mtdname="$1"
	local offset="$2"
	local ubidev part

	. /lib/upgrade/nand.sh

	ubidev=$(nand_find_ubi "$CI_UBIPART")
	part=$(nand_find_volume "$ubidev" "$mtdname")
	get_mac_binary "/dev/$part" "$offset"
}

mtd_get_part_size() {
	local mtdname="$1"
	local dev size erasesize name

	while read dev size erasesize name; do
		[[ "$name" == *"$mtdname"* ]] && echo -n $((0x${size})) && break
	done < /proc/mtd
}

mmc_get_mac_binary() {
	local part_name="$1"
	local offset="$2"
	local part

	part=$(find_mmc_part "$part_name")
	get_mac_binary "$part" "$offset"
}

macaddr_add() {
	local mac="$1"
	local val="${2:-1}"
	local oui nic

	oui=$(macaddr_octet "$mac" 0)
	nic=$(macaddr_octet "$mac" 3)
	macaddr_canonicalize "${oui}$(printf '%06x' $((0x${nic} + val & 0xffffff)))"
}

macaddr_octet() {
	local mac="$1"
	local off="${2:-3}"
	local len="${3:-3}"
	local sep="$4"
	local ret=""

	while [ "$off" -ne 0 ]; do
		mac="${mac#*$delimiter}"
		off=$((off - 1))
	done

	while [ "$len" -ne 0 ]; do
		ret="${ret}${ret:+$sep}${mac%%$delimiter*}"
		mac="${mac#*$delimiter}"
		len=$((len - 1))
	done

	echo -n "$ret"
}

macaddr_setbit() {
	local hex="${1//$delimiter/}"
	local bit="${2:-0}"

	[ "$bit" -gt 0 ] && [ "$bit" -le 48 ] || return

	macaddr_canonicalize "$(printf '%012x' $((0x${hex} | (2**(48 - bit)))))"
}

macaddr_unsetbit() {
	local hex="${1//$delimiter/}"
	local bit="${2:-0}"

	[ "$bit" -gt 0 ] && [ "$bit" -le 48 ] || return

	macaddr_canonicalize "$(printf '%012x' $((0x${hex} & ~(2**(48 - bit)))))"
}

macaddr_setbit_la() {
	macaddr_setbit "$1" 7
}

macaddr_unsetbit_mc() {
	macaddr_unsetbit "$1" 8
}

macaddr_random() {
	local randmac

	randmac=$(get_mac_binary "/dev/urandom")

	macaddr_setbit_la $(macaddr_unsetbit_mc "$randmac")
}

macaddr_2bin() {
	echo -n -e "\\x${1//$delimiter/\\x}"
}

macaddr_canonicalize() {
	local mac="$1"
	local canon=""
	local hex="${mac//$delimiter/}"

	mac="${mac//$blockchar/}"
	mac="${mac//$operators/}"

	[ "${#hex}" -gt 0 ] && [ "${#hex}" -le 12 ] && [ -z "${hex//$hexadecimal/}" ] || return

	for octet in ${mac//$delimiter/ }; do
		case "${#octet}" in
		1)
			octet="0${octet}"
			;;
		2)
			;;
		4)
			octet="${octet:0:2} ${octet:2:2}"
			;;
		6)
			octet="${octet:0:2} ${octet:2:2} ${octet:4:2}"
			;;
		8)
			octet="${octet:0:2} ${octet:2:2} ${octet:4:2} ${octet:6:2}"
			;;
		10)
			octet="${octet:0:2} ${octet:2:2} ${octet:4:2} ${octet:6:2} ${octet:8:2}"
			;;
		12)
			octet="${octet:0:2} ${octet:2:2} ${octet:4:2} ${octet:6:2} ${octet:8:2} ${octet:10:2}"
			;;
		*)
			return
			;;
		esac
		canon="${canon}${canon:+ }${octet}"
	done

	[ "${#canon}" -eq 17 ] || return

	echo -n "${canon//$delimiter/:}" | tr "$hexadecimal" "$hexcanonical"
}
