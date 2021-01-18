#!/usr/bin/env python3

# python3 convert-frames.py <frames-folder> 
#--------------------------------------------
# This script converts all JPG files in the
# specified folder to a custom (and terrible)
# format. The resulting file is saved as
# video.bin. This file is then compressed with
# zlib to create video.bin.zlib. This compressed
# file is then used to create an object file
# which can be passed to clang.
#
# At the end, video.h and video.o are created.
# These files are used to embed the input frames
# to the module.
#
# NOTE: This module only supports monochrome
# videos. If you pass it non-monochrome frames,
# only the R (red) components will be used to
# create the module files.

from PIL import Image
from sys import argv
from math import floor
from os import listdir, system, unlink
from os.path import getsize

output_file = open("video.bin", "wb")

decompressed_size = 0

frame_count = 0
frame_width = 0
frame_height = 0

current_byte = 0
byte_count = 0

def commit_byte():
	global decompressed_size
	global output_file
	global current_byte
	global byte_count
	if byte_count == 0:
		return
	output_file.write(bytes([
		byte_count & 0xFF,
		(byte_count >> 8) & 0xFF,
		(byte_count >> 16) & 0xFF,
		(byte_count >> 24) & 0xFF,
		current_byte
	]))
	decompressed_size += 5

def push_byte(byte):
	global byte_count
	global current_byte
	global output_file
	
	if (byte_count != 0) and (current_byte == byte):
		byte_count += 1
	else:
		commit_byte()
		byte_count = 1
		current_byte = byte

def convert_frame(path):
	global frame_height
	global frame_count
	global frame_width
	image = Image.open(path).convert("RGB")

	for x in range(image.width):
		for y in range(image.height):
			pos = (x, y)
			pixel = image.getpixel(pos)
			push_byte(pixel[0])
	
	image_size = (image.width, image.height)
	frame_count += 1
	if (frame_height + frame_width) != 0 and (image_size[0] != frame_width or image_size[1] != frame_height):
		print("All frames must have the same size")
		exit(1)
	frame_width = image_size[0]
	frame_height = image_size[1]

def convert_frames(dir_path):
	global frame_count
	dir_list = listdir(dir_path)
	dir_list.sort()

	for relative_path in dir_list:
		if relative_path.endswith(".jpg"):
			print(relative_path)
			convert_frame(f'{argv[1]}/{relative_path}')
			if frame_count == 300:
				break

convert_frames(argv[1])
commit_byte()

print(f'Count: {frame_count}')
print(f'Size (1 frame): {frame_width}x{frame_height}')

output_file.close()

# zlib-flate is a part of qpdf

print("Compressing video.bin...")
system("zlib-flate -compress=9 < video.bin > bad_apple_compressed")
system("xxd -i bad_apple_compressed > video.c")
compressed_size = getsize("bad_apple_compressed")
unlink("bad_apple_compressed")
unlink("video.bin")

output_file = open("video.c", "at")
output_file.write(f'\n#include \"pongo/pongo.h\"\nuint32_t bad_apple_frame_count = {frame_count};\nuint32_t bad_apple_frame_width = {frame_width};\nuint32_t bad_apple_frame_height = {frame_height};\nsize_t bad_apple_decompressed_size = {decompressed_size};')
output_file.close()

print("Creating object file...")
system("xcrun -sdk iphoneos gcc video.c -c -arch arm64 -mabi=aapcs -nostdlib -O3")

unlink("video.c")

print("Creating header file...")
output_file = open("video.h", "wt")
output_file.write(f'#include \"pongo.h\"\nuint32_t bad_apple_frame_count;\nuint32_t bad_apple_frame_width;\nuint32_t bad_apple_frame_height;\nsize_t bad_apple_decompressed_size;\nuint8_t bad_apple_compressed[{compressed_size}];')
output_file.close()