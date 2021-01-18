# bad_apple module

This is a simple pongoOS module for playing monochrome videos in pongoOS. The code does work but it is extremely inefficient and needs to be worked on. For example, generating a module from a ~2 MB mp4 file takes at least an hour and the resulting module is 89 MBs in size.

## Building

1. If you want to, create your own video files with `convert-frames.py`. If you skip this step, Bad Apple frames in the `bad-apple` folder will be used instead.
2. Make sure you cloned pongoOS to `../pongoOS` and compiled it.
3. Connect your iDevice with pongoOS booted up.
4. Run `make badapple.bin test`.

## Files

### `./convert-frames.py <frames>`

You can use this script to generate your own video files. See the comment at the top of the file for more information.

### `bad-apple`

This folder contains data for the Bad Apple video. See the [README.md file](bad-apple/README.md) for more information.

### `pongo`, `uzlib`

These folders contain the headers for pongoOS and uzlib.

### `uzlib.a`

This is a prebuilt archive for uzlib. The source code was not changed and can be found [here](https://github.com/pfalcon/uzlib/tree/27e4f4c15ba30c2cfc89575159e8efb50f95037e).