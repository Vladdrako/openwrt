# Copyright (C) 2006-2013 OpenWrt.org

. /lib/functions.sh

get_mac_binary() {
	local path="$1"
	local offset="${2:-0}"
	local length="${3:-6}"

	if ! [ -e "$path" ]; then
		echo "get_mac_binary: file $path not found!" >&2
		return
	fi

	hexdump -v -s "$offset" -n "$length" -e '6/1 "%02x"' "$path" |
		macaddr_canonicalize
}

get_mac_label_dt() {
	local basepath="/proc/device-tree"
	local macdevice mac

	macdevice=$(cat "$basepath/aliases/label-mac-device" 2>/dev/null)
	[ -n "$macdevice" ] || return

	mac=$(get_mac_binary "$basepath/$macdevice/mac-address")
	[ -n "$mac" ] &&
		echo -n "$mac" ||
		echo -n $(get_mac_binary "$basepath/$macdevice/local-mac-address")
}

get_mac_label_json() {
	local cfg="/etc/board.json"
	local mac

	[ -s "$cfg" ] || return

	. /usr/share/libubox/jshn.sh

	json_init
	json_load "$(cat $cfg)"
	if json_is_a system object; then
		json_select system
			json_get_var mac label_macaddr
		json_select ..
	fi

	echo -n "$mac"
}

get_mac_label() {
	local mac

	mac=$(get_mac_label_dt)
	[ -n "$mac" ] &&
		echo -n "$mac" ||
		echo -n "$(get_mac_label_json)"
}

find_mtd_chardev() {
	local mtdname="$1"
	local prefix index

	index=$(find_mtd_index "$mtdname")
	[ -d /dev/mtd ] && prefix="/dev/mtd/" || prefix="/dev/mtd"

	echo -n "${index:+${prefix}${index}}"
}

mtd_get_mac_ascii() {
	local mtdname="$1"
	local key="$2"
	local cmd="$3"
	local part keys var mac

	part=$(find_mtd_part "$mtdname")
	if [ -z "$part" ]; then
		echo "mtd_get_mac_ascii: partition $mtdname not found!" >&2
		return
	fi

	[ -z "$cmd" ] &&
		case "$mtdname" in
			"u"*"boot"*"env") cmd="tail -c +5" ;;
			*) cmd="cat" ;;
		esac

	keys=$($cmd "$part" | strings -n 12 -)

	for var in $keys; do
		case "$var" in
			"$key") mac="$var" && break ;;
			*"$key"*) mac="$var" && continue ;;
		esac
	done

	mac="${mac//${key}*${operators}/}"
	mac="${mac//${operators}*${key}/}"

	echo -n "${mac//$key/}" | macaddr_canonicalize
}

mtd_get_mac_text() {
	local mtdname="$1"
	local offset=$((${2:-0}))
	local length="${3:-17}"
	local part

	part=$(find_mtd_part "$mtdname")
	if [ -z "$part" ]; then
		echo "mtd_get_mac_text: partition $mtdname not found!" >&2
		return
	fi

	head -c $((offset + length)) "$part" | tail -c "$length" |
		macaddr_canonicalize
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
		case "$name" in
			*"$mtdname"*) echo -n $((0x${size})) && break ;;
		esac
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
	printf '%s%06x' "$oui" $((0x${nic} + val & 0xffffff)) | macaddr_canonicalize
}

macaddr_octet() {
	local mac="$1"
	local off="${2:-3}"
	local len="${3:-3}"
	local sep="$4"
	local oct=""

	while [ "$off" -ne 0 ]; do
		mac="${mac#*$delimiter}"
		off=$((off - 1))
	done

	while [ "$len" -ne 0 ]; do
		oct="${oct}${oct:+$sep}${mac%%$delimiter*}"
		mac="${mac#*$delimiter}"
		len=$((len - 1))
	done

	echo -n "$oct"
}

macaddr_setbit() {
	local hex="${1//$delimiter/}"
	local bit="${2:-0}"

	[ "$bit" -ge 0 ] && [ "$bit" -lt 48 ] || return

	printf '%012x' $((0x${hex} | (2**((${#hex} * 4 - 1) - bit)))) | macaddr_canonicalize
}

macaddr_unsetbit() {
	local hex="${1//$delimiter/}"
	local bit="${2:-0}"

	[ "$bit" -ge 0 ] && [ "$bit" -lt 48 ] || return

	printf '%012x' $((0x${hex} &~(2**((${#hex} * 4 - 1) - bit)))) | macaddr_canonicalize
}

macaddr_setbit_la() {
	local mac="$1"
	local stdin; read -t 1 stdin

	[ -z "$mac" ] && mac="$stdin"

	macaddr_setbit "$mac" 6
}

macaddr_unsetbit_mc() {
	local mac="$1"
	local stdin; read -t 1 stdin

	[ -z "$mac" ] && mac="$stdin"

	macaddr_unsetbit "$mac" 7
}

macaddr_random() {
	local randmac

	randmac=$(get_mac_binary "/dev/urandom")
	echo -n "$randmac" | macaddr_unsetbit_mc | macaddr_setbit_la
}

macaddr_2bin() {
	local mac="$1"
	local stdin; read -t 1 stdin

	[ -z "$mac" ] && mac="$stdin"

	printf '%b' "\\x${mac//$delimiter/\\x}"
}

macaddr_split() {
	local sub="$1"
	local mac

	[ $((${#sub} % 2)) -eq 1 ] &&
		mac="${sub%${sub#?}}" && sub="${sub#?}"

	while [ "${#sub}" -gt 2 ]; do
		mac="${mac}${mac:+ }${sub%${sub#??}}"
		sub="${sub#??}"
	done

	echo -n "${mac} ${sub%${sub#??}}"
}

macaddr_canonicalize() {
	local mac="$1"
	local hex octet octets canon
	local stdin; read -t 1 stdin

	[ -z "$mac" ] && mac="$stdin"

	mac="${mac//$blockchar/}"
	mac="${mac//$operators/}"
	hex="${mac//$delimiter/}"

	[ "${#hex}" -gt 10 ] && [ -z "${hex//$hexadecimal/}" ] || return
	[ "${#hex}" -gt 12 ] && mac=$(echo "$hex" | head -c 12)

	for octet in ${mac//$delimiter/ }; do
		[ "${#octet}" -eq 2 ] ||
			octet=$(macaddr_split "$octet")
		octets="${octets}${octets:+ }${octet}"
	done

	for octet in $octets; do
		canon="${canon}${canon:+ 0x}${octet}"
	done

	printf '%02x:%02x:%02x:%02x:%02x:%02x' $canon
}
