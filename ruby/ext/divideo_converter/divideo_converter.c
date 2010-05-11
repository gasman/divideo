#include "divideo_converter.h"

#include <stddef.h>

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
