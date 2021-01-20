#!/usr/bin/env bash

# Fail on error
set -e

if [ -z "$1" ]; then
	echo "Usage: $0 <gif>" >&2
	exit 1
fi

# pv makes it possible to view the progress of the script. You can install
# it with one of the following commands:
#   sudo port install pv   # MacPorts
#   brew install pv        # brew.sh
pv() {
	if command type pv 2> /dev/null >&2; then command pv "$@"; else cat "$@"; fi
}

file_size="$(wc -c < "$1" | awk '{print $1}')"

echo "Converting GIF to a C file..."

# This method is significantly faster than xxd -i
printf '#include <stdint.h>\nuint8_t bundled_gif[] =\n' > video.c
pv "$1" | hexdump -v -e '16/1 "_x%02X" "\n"' | sed 's/_/\\/g; s/\\x  //g; s/.*/    "&"/' >> video.c
printf ';' >> video.c

echo "Creating the object file..."
xcrun -sdk iphoneos gcc -c -arch arm64 -mabi=aapcs -nostdlib -O3 video.c
rm video.c

echo "Creating the header file..."
cat > video.h <<EOF
#ifndef __BAD_APPLE_VIDEO_H
#define __BAD_APPLE_VIDEO_H

#include <stdint.h>
uint8_t bundled_gif[${file_size}];

#endif
EOF