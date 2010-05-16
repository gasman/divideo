#ifndef DIVIDEO_CONVERTER_H
#define DIVIDEO_CONVERTER_H

#include <stddef.h>
#include <stdio.h>
#include "delta_compressor.h"
#include "video_reader.h"
#include "image_to_spectrum.h"

typedef struct st_video_writer {
	FILE *file;
	char internal_filename[32];
	int has_written_header;
	
	unsigned char *current_frame;
	unsigned char *prev_frame;
	unsigned char *prev_prev_frame;
	
	unsigned char *current_frame_pack_data;
	int current_frame_pack_data_size;
	unsigned char *prev_frame_pack_data;
	int prev_frame_pack_data_size;
	
	long max_frames;
	double volume_boost;
	double contrast;
	double brightlevel;
	char border_colour;
	st_converter_data zx_converter;
} video_writer;

size_t divideo_converter_encode_deltas(st_delta *deltas, size_t delta_count, unsigned char *frame, double frame_start_time, st_video_reader_data *video, double volume_boost, unsigned char *output);

void video_writer_init();
void video_writer_create(video_writer *vwriter, char *filename, char *internal_filename, int use_colour, int dither_map_size);
int video_writer_append(video_writer *vwriter, char *filename);
void video_writer_close(video_writer *vwriter);
void video_writer_destroy(video_writer *vwriter);

#endif