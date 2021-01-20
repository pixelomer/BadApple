#include "pongo/pongo.h"
//#include "uzlib/uzlib.h"
#include <stdbool.h>
#include "video.h"
#include "gifdec.h"

#define ENABLE_SCALING 0

#define min(x,y) (((x)>(y))?(y):(x))

void *malloc(size_t nbytes);
void free(void *mem);

void play_bad_apple() {
	gd_GIF *gif = gd_open_gif(bundled_gif, sizeof(bundled_gif));
	if (!gif) {
		puts("Failed to initialize GIF structure");
		return;
	}

	uint8_t *frame = malloc(gif->width * gif->height * 3);
	if (!frame) {
		puts("Failed to allocate memory for the frame buffer");
		gd_close_gif(gif);
		return;
	}

	// Initialize screen variables
	#if ENABLE_SCALING
	const uint32_t multiplier = min(
		(gHeight / bad_apple_frame_height),
		(gWidth / bad_apple_frame_width)
	);
	#else
	const uint32_t multiplier = 1;
	#endif
	printf("Screen multiplier: %u\n", multiplier);
	uint32_t baseX = (gWidth - (gif->width * multiplier)) / 2;
	uint32_t baseY = (gHeight - (gif->height * multiplier)) / 2;

	// Clear the display
	puts("Clearing display...");
	for (uint32_t x=0; x<gWidth; x++) {
		for (uint32_t y=0; y<gHeight; y++) {
			const uint32_t border_size = 3;
			if (
				(x >= (baseX - border_size)) &&
				(y >= (baseY - border_size)) &&
				(x <= (baseX + (gif->width * multiplier) + border_size - 1)) &&
				(y <= (baseY + (gif->height * multiplier) + border_size - 1))
			) {
				gFramebuffer[gRowPixels * y + x] = 0xFFFFFFFF;
			}
			else {
				gFramebuffer[gRowPixels * y + x] = 0x00000000;
			}
		}
	}

	// Play the GIF
	while (true) {
		// Get the start time for this frame. This is necessary
		// to prevent the video from being played slower than
		// normal.
		uint64_t draw_start = get_ticks();

		// Get the next frame
		if (gd_get_frame(gif) != 1) {
			break;
		}
		gd_render_frame(gif, frame);
		
		// Loop over every position on the screen
		uint8_t *current_color = frame;
		for (uint32_t y=0; y<gif->height; y++) {
			for (uint32_t x=0; x<gif->width; x++) {
				uint32_t color = (
					(*(current_color)     << 22) |
					(*(current_color + 1) << 12) |
					(*(current_color + 2) <<  2) |
					0b11
				);
				#if ENABLE_SCALING
				for (uint32_t sy=y; sy < (y + 1) * multiplier; sy++) {
					for (uint32_t sx=x; sx < (x + 1) * multiplier; sx++) {
						gFramebuffer[gRowPixels * (baseY + sy) + (baseX + sx)] = color;
					}
				}
				#else
				gFramebuffer[gRowPixels * (baseY + y) + (baseX + x)] = color;
				#endif
				current_color += 3;
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
	gd_close_gif(gif);
	free(frame);
	queue_rx_string("fbclear\n");
}

void module_entry() {
	command_register("bad_apple", "plays bundled video", play_bad_apple);
}

char *module_name = "bad_apple";
struct pongo_exports exported_symbols[] = {
	{.name = 0, .value = 0}
};