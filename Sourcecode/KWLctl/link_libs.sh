#!/bin/sh

# This script links Arduino libraries to libraries in the project, so that Arduino IDE
# can compile the project in the first place. Unfortunately, it's not possible to reference
# libraries in the project directly, only via symlinks.

ARDUINO_LIBS_DIR=~/Documents/Arduino/libraries
cd `dirname $0`
ROOT=`pwd`
echo "Linking Arduino libraries to libraries in source code"

for i in $ROOT/libraries/*; do
    LIBNAME="`basename $i`"
    DEST="$ARDUINO_LIBS_DIR/$LIBNAME"
    SRC="$i"
    echo "  - linking $LIBNAME to $DEST"
    if [ -L "$DEST" ]; then
        echo "  - WARNING: Library $LIBNAME already exists, relinking"
        if ! (rm $DEST;  ln -s "$SRC" "$DEST"); then
            echo "  - ERROR: cannot relink library $SRC to $DEST"
        fi
    elif [ -d "$DEST" ]; then
        echo "  - ERROR: Library $LIBNAME already exists as real directory, please check"
    else
        if ! ln -s "$SRC" "$DEST"; then
            echo "  - ERROR: cannot link library $SRC to $DEST"
        fi
    fi
done
