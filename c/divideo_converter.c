#include "divideo_converter.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video_reader.h"
#include "image_preprocessor.h"
#include "image_to_spectrum.h"
#include "delta_compressor.h"

#define REAL_FRAME_TIME 141816 /* real length of a frame; used for audio timings */

static int audio_level_to_ay_level(long long v0, double volume_boost) {
	double v = v0 * volume_boost;
	
	if (v < 225) { return 0; }
	else if (v < 560) { return 1; }
	else if (v < 811) { return 2; }
	else if (v < 1169) { return 3; }
	else if (v < 1706) { return 4; }
	else if (v < 2401) { return 5; }
	else if (v < 3631) { return 6; }
	else if (v < 5014) { return 7; }
	else if (v < 7107) { return 8; }
	else if (v < 10114) { return 9; }
	else if (v < 13150) { return 10; }
	else if (v < 16716) { return 11; }
	else if (v < 20605) { return 12; }
	else if (v < 25156) { return 13; }
	else if (v < 30279) { return 14; }
	else { return 15; }
}

size_t divideo_converter_encode_deltas(st_delta *deltas, size_t delta_count, unsigned char *frame, double frame_start_time, st_video_reader_data *video, double volume_boost, unsigned char *output) {
	unsigned char *output_pos = output;
	
	size_t i;
	long long original_audio_level;
	
	for (i = 0; i < delta_count; i++) {
		if (i+1 < delta_count) {
			original_audio_level = video_reader_average_audio_level(video, frame_start_time + deltas[i+1].time);
		} else {
			original_audio_level = video_reader_average_audio_level(video, frame_start_time + REAL_FRAME_TIME);
		}
		int audio_level = audio_level_to_ay_level(original_audio_level, volume_boost);
		int start_addr = deltas[i].offset + 0xc000;
		
		*output_pos = start_addr & 0xff; output_pos++;
		*output_pos = start_addr >> 8; output_pos++;
		*output_pos = (254 - deltas[i].length * 2); output_pos++;
		*output_pos = 0x80 + audio_level; output_pos++;
		
		memcpy(output_pos, frame + deltas[i].offset, deltas[i].length); output_pos += deltas[i].length;
	}
		
	return output_pos - output;
}

void video_writer_init() {
	video_reader_init();
}

void video_writer_create(video_writer *vwriter, char *filename, char *internal_filename, int use_colour, int dither_map_size) {
	vwriter->file = fopen(filename, "wb");
	memset(vwriter->internal_filename, 0, 32);
	strncpy(vwriter->internal_filename, internal_filename, 32);
	vwriter->current_frame = calloc(0x1b00, sizeof(unsigned char));
	vwriter->prev_frame = calloc(0x1b00, sizeof(unsigned char));
	vwriter->prev_prev_frame = calloc(0x1b00, sizeof(unsigned char));
	vwriter->current_frame_pack_data = malloc(100000);
	vwriter->prev_frame_pack_data = malloc(100000);
	vwriter->prev_frame_pack_data_size = 0;
	vwriter->has_written_header = 0;
	vwriter->max_frames = -1;
	vwriter->volume_boost = 1.0;
	vwriter->contrast = -1.0;
	vwriter->brightlevel = -1.0;
	image_to_spectrum_converter_init( &(vwriter->zx_converter), use_colour, dither_map_size );
}

int video_writer_append(video_writer *vwriter, char *filename) {
	long frame_number = 0;
	int frame_ppm_size;
	static char bitmap[256*192*4];
	static st_delta deltas[10000];
	int delta_count;
	int i;
	
	st_video_reader_data vrdata;
	
	video_reader_init_data(&vrdata);
	int error = video_reader_open(&vrdata, filename);
	if (error < 0) return error;

	while(1) {
		char *frame_ppm = video_reader_read_ppm(&vrdata, &frame_ppm_size);
		frame_number++;
		if (frame_ppm == NULL) break;
		if (vwriter->max_frames > 0 && frame_number > vwriter->max_frames) break;
		image_preprocessor_ppm_to_bitmap(frame_ppm, frame_ppm_size, vwriter->contrast, vwriter->brightlevel, bitmap);
		free(frame_ppm);
		image_to_spectrum_convert( &(vwriter->zx_converter), (unsigned char *)bitmap, vwriter->current_frame );
		printf("frame %ld\t", frame_number);
		delta_compressor_compute_optimal_deltas( vwriter->prev_prev_frame, vwriter->current_frame, deltas, &delta_count);
		
		vwriter->current_frame_pack_data_size = divideo_converter_encode_deltas(
			deltas, delta_count, vwriter->current_frame,
			frame_number * REAL_FRAME_TIME, &vrdata, vwriter->volume_boost, vwriter->current_frame_pack_data );
		
		/* calculate number of sectors in this frame; 4 bytes will be added for the end marker,
		so calculate as (number of complete sectors when three bytes are added) plus one */
		int current_frame_sector_size = ((vwriter->current_frame_pack_data_size + 3) >> 9) + 1;
		
		if ( !vwriter->has_written_header ) {
			/* write header */
			fputs( "DivIDEog\x01", vwriter->file );
			fputc( current_frame_sector_size, vwriter->file );
			fputc( vwriter->border_colour, vwriter->file );
			for (i = 0; i < 21; i++) {
				fputc( 0, vwriter->file );
			}
			fwrite( vwriter->internal_filename, sizeof(char), 32, vwriter->file );
			for (i = 0; i < 0x1c0; i++) {
				fputc( 0, vwriter->file );
			}
			
			vwriter->has_written_header = 1;
		} else {
			/* write previous frame */
			fwrite( vwriter->prev_frame_pack_data, sizeof(char),
				vwriter->prev_frame_pack_data_size, vwriter->file );
			fputc( current_frame_sector_size, vwriter->file );
			fputc( 0, vwriter->file );
			fputc( 0, vwriter->file );
			fputc( 0, vwriter->file );
			for (i = vwriter->prev_frame_pack_data_size + 4; i % 512 != 0; i++) {
				fputc( 0, vwriter->file );
			}
		}
		/* rotate packdata buffers */
		unsigned char *tmp;
		tmp = vwriter->prev_frame_pack_data;
		vwriter->prev_frame_pack_data = vwriter->current_frame_pack_data;
		vwriter->prev_frame_pack_data_size = vwriter->current_frame_pack_data_size;
		vwriter->current_frame_pack_data = tmp;
		
		/* rotate frame buffers */
		tmp = vwriter->prev_prev_frame;
		vwriter->prev_prev_frame = vwriter->prev_frame;
		vwriter->prev_frame = vwriter->current_frame;
		vwriter->current_frame = tmp;
	}
	
	video_reader_close_data(&vrdata);
	return 0;
}

void video_writer_close(video_writer *vwriter) {
	/* write last frame */
	fwrite( vwriter->prev_frame_pack_data, sizeof(char),
		vwriter->prev_frame_pack_data_size, vwriter->file );
	fputc( 0, vwriter->file );
	fputc( 0, vwriter->file );
	fputc( 0, vwriter->file );
	fputc( 0, vwriter->file );
	
	fclose( vwriter->file );
}

void video_writer_destroy(video_writer *vwriter) {
	free( vwriter->current_frame );
	free( vwriter->prev_frame );
	free( vwriter->prev_prev_frame );
	free( vwriter->current_frame_pack_data );
	free( vwriter->prev_frame_pack_data );

}
