/* 
	gifdec.c created by lecram
	License: Public domain
	Original source:
		https://github.com/lecram/gifdec
	Modified by pixelomer for use in BadApple:
		https://github.com/pixelomer/gifdec
*/

#include "gifdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fprintf(...)

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

off_t gif_seek_buffer(gd_GIF *gif, off_t offset, int whence) {
	fprintf(stderr, "gif_seek_buffer(%p, %lld, %d)\n", gif, offset, whence);
	fprintf(stderr, "Seeking from %llu\n", gif->buffer_offset);
	switch (whence) {
		case SEEK_SET:
			gif->buffer_offset = offset;
			break;
		case SEEK_CUR:
			if (-offset >= (off_t)gif->buffer_size) {
				gif->buffer_offset = 0;
			}
			else {
				gif->buffer_offset += offset;
			}
			break;
		case SEEK_END:
			if (offset >= (off_t)gif->buffer_size) {
				gif->buffer_offset = 0;
			}
			else {
				gif->buffer_offset = gif->buffer_size - offset - 1;
			}
			break;
		default:
			return -1;
	}
	if (gif->buffer_offset >= gif->buffer_size) {
		gif->buffer_offset = gif->buffer_size - 1;
	}
	fprintf(stderr, "Seeked to %llu\n", gif->buffer_offset);
	return (off_t)gif->buffer_offset;
}

ssize_t gif_read_buffer(gd_GIF *gif, void *buffer, size_t nbytes) {
	size_t read_amount = MIN(nbytes, (gif->buffer_size - gif->buffer_offset));
	memcpy(buffer, gif->buffer + gif->buffer_offset, read_amount);
	fprintf(stderr, "gif_read_buffer(%p, %p, %zu)\n", gif, buffer, nbytes);
	fprintf(stderr, "Read %zu bytes from offset %llu\n", read_amount, gif->buffer_offset);
	gif->buffer_offset += read_amount;
	return (ssize_t)read_amount;
}

#define lseek(gif, offset, whence) gif_seek_buffer(gif, offset, whence)
#define read(gif, buffer, len) gif_read_buffer(gif, buffer, len)

typedef struct Entry {
	uint16_t length;
	uint16_t prefix;
	uint8_t  suffix;
} Entry;

typedef struct Table {
	int bulk;
	int nentries;
	Entry *entries;
} Table;

static uint16_t
read_num(gd_GIF *gif)
{
	uint8_t bytes[2];
	read(gif, bytes, 2);
	return bytes[0] | (((uint16_t) bytes[1]) << 8);
}

gd_GIF *
gd_open_gif(void *buffer, size_t len)
{
	uint8_t sigver[3];
	uint16_t depth;
	uint8_t fdsz, bgidx, aspect;
	int i;
	uint8_t *bgcolor;
	int gct_sz;

	gd_GIF *gif = malloc(sizeof(*gif));
	if (!gif) goto fail;
	gif->buffer_offset = 0;
	gif->buffer_size = len;
	gif->buffer = (uint8_t *)buffer;

	/* Header */
	read(gif, sigver, 3);
	if (memcmp(sigver, "GIF", 3) != 0) {
		fprintf(stderr, "invalid signature\n");
		goto fail;
	}
	/* Version */
	read(gif, sigver, 3);
	if (memcmp(sigver, "89a", 3) != 0) {
		fprintf(stderr, "invalid version\n");
		goto fail;
	}
	/* Width x Height */
	gif->width  = read_num(gif);
	gif->height = read_num(gif);
	/* FDSZ */
	read(gif, &fdsz, 1);
	/* Presence of GCT */
	if (!(fdsz & 0x80)) {
		fprintf(stderr, "no global color table\n");
		goto fail;
	}
	/* Color Space's Depth */
	depth = ((fdsz >> 4) & 7) + 1;
	/* Ignore Sort Flag. */
	/* GCT Size */
	gct_sz = 1 << ((fdsz & 0x07) + 1);
	/* Background Color Index */
	read(gif, &bgidx, 1);
	/* Aspect Ratio */
	read(gif, &aspect, 1);

	gd_GIF *final_gif = realloc(gif, sizeof(*gif) + 4 * gif->width * gif->height);
	if (!final_gif) goto fail;
	gif = final_gif;

	gif->depth  = depth;
	/* Read GCT */
	gif->gct.size = gct_sz;
	read(gif, gif->gct.colors, 3 * gif->gct.size);
	gif->palette = &gif->gct;
	gif->bgindex = bgidx;
	gif->canvas = (uint8_t *)(gif + 1);
	gif->frame = &gif->canvas[3 * gif->width * gif->height];
	if (gif->bgindex)
		memset(gif->frame, gif->bgindex, gif->width * gif->height);
	
	bgcolor = &gif->palette->colors[gif->bgindex*3];

	if (bgcolor[0] || bgcolor[1] || bgcolor [2])
		for (i = 0; i < gif->width * gif->height; i++)
			memcpy(&gif->canvas[i*3], bgcolor, 3);

	gif->anim_start = gif->buffer_offset;

	goto ok;
fail:
	if (gif) {
		free(gif);
		gif = NULL;
	}
ok:
	return gif;
}

static void
discard_sub_blocks(gd_GIF *gif)
{
	uint8_t size;

	do {
		read(gif, &size, 1);
		lseek(gif, size, SEEK_CUR);
	} while (size);
}

static void
read_plain_text_ext(gd_GIF *gif)
{
	if (gif->plain_text) {
		uint16_t tx, ty, tw, th;
		uint8_t cw, ch, fg, bg;
		off_t sub_block;
		lseek(gif, 1, SEEK_CUR); /* block size = 12 */
		tx = read_num(gif);
		ty = read_num(gif);
		tw = read_num(gif);
		th = read_num(gif);
		read(gif, &cw, 1);
		read(gif, &ch, 1);
		read(gif, &fg, 1);
		read(gif, &bg, 1);
		sub_block = lseek(gif, 0, SEEK_CUR);
		gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
		lseek(gif, sub_block, SEEK_SET);
	} else {
		/* Discard plain text metadata. */
		lseek(gif, 13, SEEK_CUR);
	}
	/* Discard plain text sub-blocks. */
	discard_sub_blocks(gif);
}

static void
read_graphic_control_ext(gd_GIF *gif)
{
	uint8_t rdit;

	/* Discard block size (always 0x04). */
	lseek(gif, 1, SEEK_CUR);
	read(gif, &rdit, 1);
	gif->gce.disposal = (rdit >> 2) & 3;
	gif->gce.input = rdit & 2;
	gif->gce.transparency = rdit & 1;
	gif->gce.delay = read_num(gif);
	read(gif, &gif->gce.tindex, 1);
	/* Skip block terminator. */
	lseek(gif, 1, SEEK_CUR);
}

static void
read_comment_ext(gd_GIF *gif)
{
	if (gif->comment) {
		off_t sub_block = lseek(gif, 0, SEEK_CUR);
		gif->comment(gif);
		lseek(gif, sub_block, SEEK_SET);
	}
	/* Discard comment sub-blocks. */
	discard_sub_blocks(gif);
}

static void
read_application_ext(gd_GIF *gif)
{
	char app_id[8];
	char app_auth_code[3];

	/* Discard block size (always 0x0B). */
	lseek(gif, 1, SEEK_CUR);
	/* Application Identifier. */
	read(gif, app_id, 8);
	/* Application Authentication Code. */
	read(gif, app_auth_code, 3);
	if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
		/* Discard block size (0x03) and constant byte (0x01). */
		lseek(gif, 2, SEEK_CUR);
		gif->loop_count = read_num(gif);
		/* Skip block terminator. */
		lseek(gif, 1, SEEK_CUR);
	} else if (gif->application) {
		off_t sub_block = lseek(gif, 0, SEEK_CUR);
		gif->application(gif, app_id, app_auth_code);
		lseek(gif, sub_block, SEEK_SET);
		discard_sub_blocks(gif);
	} else {
		discard_sub_blocks(gif);
	}
}

static void
read_ext(gd_GIF *gif)
{
	uint8_t label;

	read(gif, &label, 1);
	switch (label) {
	case 0x01:
		read_plain_text_ext(gif);
		break;
	case 0xF9:
		read_graphic_control_ext(gif);
		break;
	case 0xFE:
		read_comment_ext(gif);
		break;
	case 0xFF:
		read_application_ext(gif);
		break;
	default:
		fprintf(stderr, "unknown extension: %02X\n", label);
	}
}

static Table *
new_table(int key_size)
{
	int key;
	int init_bulk = MAX(1 << (key_size + 1), 0x100);
	Table *table = malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
	if (table) {
		table->bulk = init_bulk;
		table->nentries = (1 << key_size) + 2;
		table->entries = (Entry *) &table[1];
		for (key = 0; key < (1 << key_size); key++)
			table->entries[key] = (Entry) {1, 0xFFF, key};
	}
	return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
	Table *table = *tablep;
	if (table->nentries == table->bulk) {
		table->bulk *= 2;
		table = realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
		if (!table) return -1;
		table->entries = (Entry *) &table[1];
		*tablep = table;
	}
	table->entries[table->nentries] = (Entry) {length, prefix, suffix};
	table->nentries++;
	if ((table->nentries & (table->nentries - 1)) == 0)
		return 1;
	return 0;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
	int bits_read;
	int rpad;
	int frag_size;
	uint16_t key;

	key = 0;
	for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
		rpad = (*shift + bits_read) % 8;
		if (rpad == 0) {
			/* Update byte. */
			if (*sub_len == 0)
				read(gif, sub_len, 1); /* Must be nonzero! */
			read(gif, byte, 1);
			(*sub_len)--;
		}
		frag_size = MIN(key_size - bits_read, 8 - rpad);
		key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
	}
	/* Clear extra bits to the left. */
	key &= (1 << key_size) - 1;
	*shift = (*shift + key_size) % 8;
	return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int
interlaced_line_index(int h, int y)
{
	int p; /* number of lines in current pass */

	p = (h - 1) / 8 + 1;
	if (y < p) /* pass 1 */
		return y * 8;
	y -= p;
	p = (h - 5) / 8 + 1;
	if (y < p) /* pass 2 */
		return y * 8 + 4;
	y -= p;
	p = (h - 3) / 4 + 1;
	if (y < p) /* pass 3 */
		return y * 4 + 2;
	y -= p;
	/* pass 4 */
	return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image_data(gd_GIF *gif, int interlace)
{
	uint8_t sub_len, shift, byte;
	int init_key_size, key_size, table_is_full;
	int frm_off, str_len, p, x, y;
	uint16_t key, clear, stop;
	int ret;
	Table *table;
	Entry entry;
	off_t start, end;

	read(gif, &byte, 1);
	key_size = (int) byte;
	start = lseek(gif, 0, SEEK_CUR);
	discard_sub_blocks(gif);
	end = lseek(gif, 0, SEEK_CUR);
	lseek(gif, start, SEEK_SET);
	clear = 1 << key_size;
	stop = clear + 1;
	table = new_table(key_size);
	key_size++;
	init_key_size = key_size;
	sub_len = shift = 0;
	key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
	frm_off = 0;
	ret = 0;
	while (1) {
		if (key == clear) {
			key_size = init_key_size;
			table->nentries = (1 << (key_size - 1)) + 2;
			table_is_full = 0;
		} else if (!table_is_full) {
			ret = add_entry(&table, str_len + 1, key, entry.suffix);
			if (ret == -1) {
				free(table);
				return -1;
			}
			if (table->nentries == 0x1000) {
				ret = 0;
				table_is_full = 1;
			}
		}
		key = get_key(gif, key_size, &sub_len, &shift, &byte);
		if (key == clear) continue;
		if (key == stop) break;
		if (ret == 1) key_size++;
		entry = table->entries[key];
		str_len = entry.length;
		while (1) {
			p = frm_off + entry.length - 1;
			x = p % gif->fw;
			y = p / gif->fw;
			if (interlace)
				y = interlaced_line_index((int) gif->fh, y);
			gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
			if (entry.prefix == 0xFFF)
				break;
			else
				entry = table->entries[entry.prefix];
		}
		frm_off += str_len;
		if (key < table->nentries - 1 && !table_is_full)
			table->entries[table->nentries - 1].suffix = entry.suffix;
	}
	free(table);
	read(gif, &sub_len, 1); /* Must be zero! */
	lseek(gif, end, SEEK_SET);
	return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image(gd_GIF *gif)
{
	uint8_t fisrz;
	int interlace;

	/* Image Descriptor. */
	gif->fx = read_num(gif);
	gif->fy = read_num(gif);
	gif->fw = read_num(gif);
	gif->fh = read_num(gif);
	read(gif, &fisrz, 1);
	interlace = fisrz & 0x40;
	/* Ignore Sort Flag. */
	/* Local Color Table? */
	if (fisrz & 0x80) {
		/* Read LCT */
		gif->lct.size = 1 << ((fisrz & 0x07) + 1);
		read(gif, gif->lct.colors, 3 * gif->lct.size);
		gif->palette = &gif->lct;
	} else
		gif->palette = &gif->gct;
	/* Image Data. */
	return read_image_data(gif, interlace);
}

static void
render_frame_rect(gd_GIF *gif, uint8_t *buffer)
{
	int i, j, k;
	uint8_t index, *color;
	i = gif->fy * gif->width + gif->fx;
	for (j = 0; j < gif->fh; j++) {
		for (k = 0; k < gif->fw; k++) {
			index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
			color = &gif->palette->colors[index*3];
			if (!gif->gce.transparency || index != gif->gce.tindex)
				memcpy(&buffer[(i+k)*3], color, 3);
		}
		i += gif->width;
	}
}

static void
dispose(gd_GIF *gif)
{
	int i, j, k;
	uint8_t *bgcolor;
	switch (gif->gce.disposal) {
	case 2: /* Restore to background color. */
		bgcolor = &gif->palette->colors[gif->bgindex*3];
		i = gif->fy * gif->width + gif->fx;
		for (j = 0; j < gif->fh; j++) {
			for (k = 0; k < gif->fw; k++)
				memcpy(&gif->canvas[(i+k)*3], bgcolor, 3);
			i += gif->width;
		}
		break;
	case 3: /* Restore to previous, i.e., don't update canvas.*/
		break;
	default:
		/* Add frame non-transparent pixels to canvas. */
		render_frame_rect(gif, gif->canvas);
	}
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int
gd_get_frame(gd_GIF *gif)
{
	char sep;

	dispose(gif);
	read(gif, &sep, 1);
	while (sep != ',') {
		if (sep == ';')
			return 0;
		if (sep == '!')
			read_ext(gif);
		else return -1;
		read(gif, &sep, 1);
	}
	if (read_image(gif) == -1)
		return -1;
	return 1;
}

void
gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
	memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
	render_frame_rect(gif, buffer);
}

int
gd_is_bgcolor(gd_GIF *gif, uint8_t color[3])
{
	return !memcmp(&gif->palette->colors[gif->bgindex*3], color, 3);
}

void
gd_rewind(gd_GIF *gif)
{
	lseek(gif, gif->anim_start, SEEK_SET);
}

void
gd_close_gif(gd_GIF *gif)
{
	free(gif);
}
