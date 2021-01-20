#!/usr/bin/env bash

# Fail on error
set -e

if [ -z "$1" ]; then
	echo "Usage: $0 <gif>" >&2
	exit 1
fi

file_size="$(wc -c < "$0" | awk '{print $1}')"

echo "Converting GIF to a C file..."
ln -s "$1" "bundled_gif"
xxd -i bundled_gif > video.c
rm bundled_gif

echo "Creating the object file..."
xcrun -sdk iphoneos gcc video.c -c -arch arm64 -mabi=aapcs -nostdlib -O3
rm video.c

echo "Creating the header file..."
cat > video.h <<EOF
#ifndef __BAD_APPLE_VIDEO_H
#define __BAD_APPLE_VIDEO_H

#include <stdint.h>
uint8_t bundled_gif[${file_size}];

#endif
EOF