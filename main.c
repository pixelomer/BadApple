#include "pongo/pongo.h"
#include "uzlib/uzlib.h"
#include <stdbool.h>
#include "video.h"

#define ENABLE_SCALING 0

#define min(x,y) (((x)>(y))?(y):(x))

void *malloc(size_t nbytes);
void free(void *mem);

void play_bad_apple() {
	// Decompress the video
	puts("Decompressing video...");
	size_t decompressed_size = bad_apple_decompressed_size;
	uint8_t *bad_apple_decompressed = malloc(decompressed_size);

	struct uzlib_uncomp d;
	uzlib_uncompress_init(&d, NULL, 0);
	d.source = bad_apple_compressed;
	d.source_limit = bad_apple_compressed + sizeof(bad_apple_compressed);
	d.source_read_cb = NULL;
	int res = uzlib_zlib_parse_header(&d);
	if (res == TINF_DATA_ERROR) {
		printf("Failed to parse header: %d\n", res);
		free(bad_apple_decompressed);
		return;
	}
	d.dest_start = bad_apple_decompressed;
	d.dest = bad_apple_decompressed;
	d.dest_limit = bad_apple_decompressed + bad_apple_decompressed_size;
	res = uzlib_uncompress_chksum(&d);
	if ((res != TINF_DONE) && (res != TINF_OK)) {
		printf("Decompress failed; expected %d, got %d instead\n", TINF_DONE, res);
		free(bad_apple_decompressed);
		return;
	}
	puts("Decompress succeeded.");

	#if ENABLE_SCALING
	const uint32_t multiplier = min(
		(gHeight / bad_apple_frame_height),
		(gWidth / bad_apple_frame_width)
	);
	#else
	const uint32_t multiplier = 1;
	#endif
	printf("Screen multiplier: %u\n", multiplier);
	uint32_t offset = 0;
	uint32_t remaining = 0;
	uint32_t color;
	uint8_t current_byte = 0;
	uint32_t baseX = (gWidth - (bad_apple_frame_width * multiplier)) / 2;
	uint32_t baseY = (gHeight - (bad_apple_frame_height * multiplier)) / 2;

	// Clear the display
	puts("Clearing display...");
	for (uint32_t x=0; x<gWidth; x++) {
		for (uint32_t y=0; y<gHeight; y++) {
			const uint32_t border_size = 3;
			if (
				(x >= (baseX - border_size)) &&
				(y >= (baseY - border_size)) &&
				(x <= (baseX + (bad_apple_frame_width * multiplier) + border_size - 1)) &&
				(y <= (baseY + (bad_apple_frame_height * multiplier) + border_size - 1))
			) {
				gFramebuffer[gRowPixels * y + x] = 0xFFFFFFFF;
			}
			else {
				gFramebuffer[gRowPixels * y + x] = 0x00000000;
			}
		}
	}

	// Play the video
	//puts("Playing video...");
	for (uint32_t frame=0; frame<bad_apple_frame_count; frame++) {
		uint64_t draw_start = get_ticks();
		bool color_changed = false;
		for (uint32_t x=0; x<bad_apple_frame_width; x++) {
			for (uint32_t y=0; y<bad_apple_frame_height; y++) {
				if (remaining == 0) {
					remaining = 0;
					remaining |= bad_apple_decompressed[offset++];
					remaining |= bad_apple_decompressed[offset++] << 8;
					remaining |= bad_apple_decompressed[offset++] << 16;
					remaining |= bad_apple_decompressed[offset++] << 24;
					current_byte = bad_apple_decompressed[offset++];
					uint32_t old_color = color;
					color = 0b11;
					// [Red][Green][Blue][Alpha]
					//  10   10     10    2
					color |= current_byte << 2;
					color |= current_byte << 12;
					color |= current_byte << 22;
					if (old_color != color) {
						color_changed = true;
					}
				}
				#if ENABLE_SCALING
				for (uint32_t sy=y; sy < (y + 1) * multiplier; sy++) {
					for (uint32_t sx=x; sx < (x + 1) * multiplier; sx++) {
						gFramebuffer[gRowPixels * (baseY + sy) + (baseX + sx)] = color;
					}
				}
				#else
				gFramebuffer[gRowPixels * (baseY + y) + (baseX + x)] = color;
				#endif
				remaining--;
			}
		}
		uint64_t draw_end = get_ticks();
		const uint32_t fps = 30;
		const uint32_t spin_duration = 1000000 / fps; // FPS = 30
		uint32_t usecs = spin_duration - ((draw_end - draw_start) / 24);
		if (usecs < spin_duration) {
			spin(usecs);
		}
	}
	free(bad_apple_decompressed);
	queue_rx_string("fbclear\n");
}

void module_entry() {
	command_register("bad_apple", "plays bundled video", play_bad_apple);
	uzlib_init();
}

char *module_name = "bad_apple";
struct pongo_exports exported_symbols[] = {
	{.name = 0, .value = 0}
};