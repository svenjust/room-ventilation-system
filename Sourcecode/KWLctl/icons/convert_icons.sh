#!/bin/sh

# This script converts icons in source directory to raw data and then packs them into a header file.

ROOT="`dirname $0`"
SRC="${ROOT}/source"
WORKDIR="${ROOT}/workdir"
mkdir -p "${WORKDIR}" || die "Cannot create working directory"

die()
{
	echo $* >&2
	exit 1
}

# Convert one icon to one or more bitmaps
#
# $1 - icon file name
# $2..n - resolutions for the icon (e.g., 32x32)
convert_icon()
{
	id=$1
	shift
	icon=$1
	name="`basename $icon`"
	name="${name%%.*}"
	shift
	opts=""
	if [[ "$1" == -* ]]; then
		opts="$1"
		shift
	fi
	while [ "$1" != "" ]; do
		res=$1
		shift
		echo "Converting $name for resolution $res"
		params="-alpha deactivate -negate -adaptive-resize $res -gravity center -background black $opts -extent $res -depth 1"
		convert "$icon" $params "${WORKDIR}/${id}_$res.gray" || die "Cannot convert icon $icon (ID $id) to size $res"
		convert "$icon" $params "${WORKDIR}/${id}_$res.png" || die "Cannot convert icon $icon (ID $id) to size $res"
	done
}

# Make data for one icon
#
# $1 icon file name
make_icon_data()
{
	name="${1%%.*}"
	name="`echo $name | tr \ - __`"
	echo "// Generated from $1"
	echo "const uint8_t icon_$name[] PROGMEM = {"
	hexdump -v -e '16/1 "0x%02x, " "\n"' "$1" | sed -e "s/ 0x  ,//g" || die "Failed converting icon $i to raw data"
	echo "};"
	echo
}

make_header()
{
	cd "${WORKDIR}"
	icon_list=`for i in *.gray; do echo $i; done | sort`
	for i in $icon_list; do
		make_icon_data $i || die "Failed generating icon data for $i"
	done
}

# convert all required icons
rm ${WORKDIR}/*.gray
convert_icon fan "${SRC}/noun_Fan_1833383.svg" 52x52 24x24 64x64 18x18
convert_icon freeze "${SRC}/noun_low_temperature_641749.svg" 64x64 56x56 24x24
convert_icon temperature "${SRC}/noun_Temperature_1365226.svg" 24x24
convert_icon humidity "${SRC}/noun_Water_927349.svg" 24x24
convert_icon co2 "${SRC}/noun_Carbon_Dioxide_183795.svg" 20x20
convert_icon voc "${SRC}/noun_molecule_669068.svg" 20x20
convert_icon bypass "${SRC}/noun_Bypass_542021.svg" -flip 64x64 56x56 24x24
convert_icon settings "${SRC}/noun_Settings_18125.svg" 56x56 24x24
convert_icon off "${SRC}/noun_off_1916005.svg" 40x40
convert_icon time "${SRC}/noun_clock_130935.svg" 56x56
convert_icon time "${SRC}/clock_24x24.png" 24x24
convert_icon network "${SRC}/noun_computer_network_1644731.svg" 56x56 24x24
convert_icon factory "${SRC}/noun_factory_1531655.svg" 56x56 24x24
convert_icon program "${SRC}/noun_agenda_63276.svg" 56x56 24x24
convert_icon back "${SRC}/noun_back_737733.svg" 56x56 32x32
convert_icon up2 "${SRC}/up2_40x40.png" -negate 40x40
convert_icon down2 "${SRC}/down2_40x40.png" -negate 40x40
convert_icon up "${SRC}/up_40x40.png" -negate 40x40
convert_icon down "${SRC}/down_40x40.png" -negate 40x40
convert_icon ok "${SRC}/noun_check_1833392.svg" 40x40
convert_icon cancel "${SRC}/cancel_40x40.png" 40x40

# generage header at the end
echo "Generating header"
make_header >"${ROOT}/../icons.h" || die "Failed generating icon header"
