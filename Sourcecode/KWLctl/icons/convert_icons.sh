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
		convert "$icon" $params "${WORKDIR}/${name}_$res.gray" || die "Cannot convert icon $icon to size $res"
		convert "$icon" $params "${WORKDIR}/${name}_$res.png" || die "Cannot convert icon $icon to size $res"
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
convert_icon "${SRC}/noun_Fan_1833383.svg" 52x52 24x24 64x64 18x18
convert_icon "${SRC}/noun_low_temperature_641749.svg" 64x64
convert_icon "${SRC}/noun_Temperature_1365226.svg" 24x24
convert_icon "${SRC}/noun_Water_927349.svg" 24x24
convert_icon "${SRC}/noun_Carbon_Dioxide_183795.svg" 20x20
convert_icon "${SRC}/noun_molecule_669068.svg" 20x20
convert_icon "${SRC}/noun_Bypass_542021.svg" -flip 64x64
convert_icon "${SRC}/noun_Settings_18125.svg" 56x56
convert_icon "${SRC}/noun_off_1916005.svg" 40x40

# generage header at the end
echo "Generating header"
make_header >"${ROOT}/../icons.h" || die "Failed generating icon header"
