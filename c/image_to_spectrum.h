#ifndef IMAGE_TO_SPECTRUM_H
#define IMAGE_TO_SPECTRUM_H

typedef struct rendering {
	unsigned char attribute;
	unsigned char ink[4];
	unsigned char paper[4];
	float threshold_map[4][4][3];
	char bitmap[8][8];
} st_rendering;

typedef struct converter_data {
	st_rendering renderings[56];
	int rendering_count;
	int (*dither_map)[4][4];
	int dither_map_size;
} st_converter_data;

void image_to_spectrum_converter_init(st_converter_data *convdata, int use_colour, int dither_map_size);
void image_to_spectrum_convert(st_converter_data *convdata, unsigned char *image_data, unsigned char *zx_data);

#endif /* IMAGE_TO_SPECTRUM_H */
