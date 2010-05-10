#include "image_to_spectrum.h"
#include <string.h>
#include <stdlib.h>

static unsigned char zx_palette[2][8][4] = {
	{"\x00\x00\x00\x00", "\x00\x00\xc0\x00", "\xc0\x00\x00\x00", "\xc0\x00\xc0\x00",
	"\x00\xc0\x00\x00", "\x00\xc0\xc0\x00", "\xc0\xc0\x00\x00", "\xc0\xc0\xc0\x00"},

	{"\x00\x00\x00\x00", "\x00\x00\xff\x00", "\xff\x00\x00\x00", "\xff\x00\xff\x00",
	"\x00\xff\x00\x00", "\x00\xff\xff\x00", "\xff\xff\x00\x00", "\xff\xff\xff\x00"}
};

int dither_map_4x4[4][4] = {
	{ 1, 9, 3,11},
	{13, 5,15, 7},
	{ 4,12, 2,10},
	{16, 8,14, 6}
};

int dither_map_2x2[4][4] = {
	{ 1, 3, 0, 0},
	{ 4, 2, 0, 0},
	{ 0, 0, 0, 0},
	{ 0, 0, 0, 0}
};

int dither_map_1x1[4][4] = {
	{ 1, 0, 0, 0},
	{ 0, 0, 0, 0},
	{ 0, 0, 0, 0},
	{ 0, 0, 0, 0}
};

static void init_rendering(st_converter_data *convdata, st_rendering *rendering, int bright, int paper, int ink) {
	int x, y, i;
	float d;
	rendering->attribute = (bright << 6) | (paper << 3) | ink;
	memcpy(rendering->ink, zx_palette[bright][ink], 4);
	memcpy(rendering->paper, zx_palette[bright][paper], 4);

	/* compute threshold map */
	for (y = 0; y < convdata->dither_map_size; y++) {
		for (x = 0; x < convdata->dither_map_size; x++) {
			for (i = 0; i < 3; i++) {
				d = (float) ((*(convdata->dither_map))[y][x]);
				d = d / (convdata->dither_map_size * convdata->dither_map_size + 1) - 0.5;
				rendering->threshold_map[y][x][i] = ((float)(rendering->paper[i] - rendering->ink[i])) * d;
			}
		}
	}
}

void image_to_spectrum_converter_init(st_converter_data *convdata, int use_colour, int dither_map_size) {
	int bright, ink, paper, i;
	
	switch (dither_map_size) {
		case 4:
			convdata->dither_map = &dither_map_4x4;
			convdata->dither_map_size = 4;
			break;
		case 2:
			convdata->dither_map = &dither_map_2x2;
			convdata->dither_map_size = 2;
			break;
		default: /* no dithering */
			convdata->dither_map = &dither_map_1x1;
			convdata->dither_map_size = 1;
			break;
	}
	
	/* initialise renderings table */
	if (use_colour) {
		i = 0;
		for (bright = 0; bright < 2; bright++) {
			for (paper = 0; paper < 7; paper++) {
				for (ink = paper+1; ink < 8; ink++) {
					init_rendering(convdata, &(convdata->renderings[i]), bright, paper, ink);
					i++;
				}
			}
		}
		convdata->rendering_count = i;
	} else {
		convdata->rendering_count = 1;
		init_rendering(convdata, &(convdata->renderings[0]), 1, 0, 7); /* bright white ink on black paper */
	}
}

static int render_char(st_converter_data *convdata, st_rendering *rendering, unsigned char* original_bitmap) {
	int y, x, i, ink_diff, paper_diff;
	unsigned char *pix;
	float adjusted_component;
	
	unsigned char *ink = rendering->ink;
	unsigned char *paper = rendering->paper;
	
	int total_diff = 0;
	
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			pix = original_bitmap + ( (y << 5) | (x << 2) );
			ink_diff = 0;
			paper_diff = 0;
			for (i = 0; i < 3; i++) {
				adjusted_component = rendering->threshold_map[y % convdata->dither_map_size][x % convdata->dither_map_size][i] + pix[i];
				ink_diff += abs(adjusted_component - ink[i]);
				paper_diff += abs(adjusted_component - paper[i]);
			}
			
			if (ink_diff < paper_diff) {
				rendering->bitmap[y][x] = 1;
				total_diff += ink_diff;
			} else {
				rendering->bitmap[y][x] = 0;
				total_diff += paper_diff;
			}
		}
	}
	return total_diff;
}

void image_to_spectrum_convert(st_converter_data *convdata, unsigned char *image_data, unsigned char *zx_data) {
	static unsigned char original_bitmap[8*8*4];
	unsigned char third, ychar, xchar, ypix, zx_byte;
	int i, rendering_closeness;
	int best_rendering_closeness;
	st_rendering *best_rendering;
	
	/* each screen third */
	for (third = 0; third < 3; third++) {
		for (ychar = 0; ychar < 8; ychar++) {
			for (xchar = 0; xchar < 32; xchar++) {
				/* copy character to original_bitmap */
				for (ypix = 0; ypix < 8; ypix++) {
					memcpy(original_bitmap + (ypix << 5), image_data + ( (third << 16) | (ychar << 13) | (ypix << 10) | (xchar << 5) ), 32);
				}
				/* render with each attribute in turn; choose the best one */
				best_rendering_closeness = 0xffffff;
				for (i = 0; i < convdata->rendering_count; i++) {
					rendering_closeness = render_char(convdata, &(convdata->renderings[i]), original_bitmap);
					if (rendering_closeness < best_rendering_closeness) {
						best_rendering_closeness = rendering_closeness;
						best_rendering = &(convdata->renderings[i]);
					}
				}
				/* convert the best rendering into Spectrum data */
				zx_data[0x1800 | (third << 8) | (ychar << 5) | xchar] = best_rendering->attribute;
				for (ypix = 0; ypix < 8; ypix++) {
					zx_byte = 0;
					for (i = 0; i < 8; i++) {
						if (best_rendering->bitmap[ypix][i]) {
							zx_byte |= (1 << (7 - i));
						}
					}
					zx_data[(third << 11) | (ypix << 8) | (ychar << 5) | xchar] = zx_byte;
				}
			}
		}
	}
}
