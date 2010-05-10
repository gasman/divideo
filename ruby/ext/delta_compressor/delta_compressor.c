#include "delta_compressor.h"
#include <string.h>
#include <stdio.h>

#define GAP_THRESHOLD 6
/* when the number of matching bytes between two runs of changes is less than
 GAP_THRESHOLD, consider them as one packet as this will have less overhead */
#define TARGET_FRAME_TIME 141000 /* minimum estimated running time for a video frame, in cycles */

#define INI_TSTATE_COUNT 16.91
	/* how long we expect an INI instruction to take - 16 without contention.
	# - Contention happens on 192/311 of the scanlines each frame;
	# - and on each of those, it is applied on 128/228 of the cycles.
	# - When this happens, there is an average of 2.625T of contention (6+5+4+3+2+1+0+0 / 8)
	# so therefore, on average an INI is contended by 2.625 * (192/311) * (128/228) => 0.9098 cycles
	*/
	
#define ROUTINE_TSTATE_COUNT 105
	/* running time of one iteration of the player routine minus INI instructions
	# (I count it as 100, but that seems to cause frames with high numbers of packets to overrun.
	# Maybe there's a contended instruction I've missed)
	*/

#define MAX_FRAME_TIME 141500 /* maximum allowable running time for a video frame, in cycles */

/* test whether the byte sequences from addr onward, up to either GAP_THRESHOLD bytes or the end of data, match */
static int sequences_match(unsigned char *old_frame, unsigned char *new_frame, size_t addr) {
	size_t end = addr + GAP_THRESHOLD;
	if (end > 0x1b00) end = 0x1b00;
	return ( memcmp(old_frame + addr, new_frame + addr, end - addr) == 0 );
}

int delta_compressor_compute_deltas(unsigned char *old_frame, unsigned char *new_frame, st_delta *deltas, int max_delta_length) {
	size_t addr = 0;
	size_t delta_offset, delta_length;
	int delta_count = 0;
	
	while (addr < 0x1b00) {
		/* scan for a difference */
		while (old_frame[addr] == new_frame[addr] && addr < 0x1b00) {
			addr++;
		}
		if (addr == 0x1b00) break;
		
		/* delta begins here */
		delta_offset = addr;
		delta_length = 0;
		
		while ( addr < 0x1b00 && delta_length < max_delta_length && !sequences_match(old_frame, new_frame, addr) ) {
			delta_length += 2; /* length of a delta must always be even */
			addr += 2;
		}
		if (delta_offset + delta_length > 0x1b00) {
			/* edge case; we've overrun the end of the screen by one byte. Start one byte earlier instead */
			delta_offset -= 1;
		}
		
		deltas[delta_count].offset = delta_offset;
		deltas[delta_count].length = delta_length;
		deltas[delta_count].time = -1;
		delta_count++;
	}
	
	return delta_count;
}

void delta_compressor_pack_deltas(st_delta *deltas, int delta_count, st_delta *packed_deltas, double *frame_time_out, int *final_delta_count_out) {
	int bytes_left_this_sector = 512;
	int remaining_deltas = delta_count;
	float frame_time = 0.0;
	int packed_delta_count = 0;
	
	int i;
	
	while (remaining_deltas > 0 || frame_time <= TARGET_FRAME_TIME) {
		/* consume the longest delta that fits inside the remaining space - but NOT one that would leave less than 4 bytes remaining */
		int chosen_delta_index = -1;
		if (remaining_deltas > 0) {
			for (i = 0; i < delta_count; i++) {
				if (deltas[i].time < 0) { /* delta has not been chosen yet */
					int spare_bytes = bytes_left_this_sector - (deltas[i].length + 4);
					if (spare_bytes == 0 || spare_bytes >= 4) {
						/* delta will fit; choose it if it's larger than any previous one found */
						if (chosen_delta_index == -1 || deltas[chosen_delta_index].length < deltas[i].length) {
							chosen_delta_index = i;
						}
					}
				}
			}
		}
		
		if (chosen_delta_index > -1) { /* a suitable delta was found */
			deltas[chosen_delta_index].time = frame_time;
			packed_deltas[packed_delta_count] = deltas[chosen_delta_index];
			remaining_deltas--;
		} else {
			/* no suitable packet, so pad with dummy data from the start of the screen instead. */
			/* TODO: fragmenting the largest packet is a better option here */
			if (remaining_deltas == 0 && bytes_left_this_sector > 7) {
				/* we're killing time until the end of the frame, so make this delta as short as possible to fit in lots of audio samples */
				packed_deltas[packed_delta_count].length = 0;
			} else {
				packed_deltas[packed_delta_count].length = bytes_left_this_sector - 4;
			}
			packed_deltas[packed_delta_count].offset = 0;
			packed_deltas[packed_delta_count].time = frame_time;
		}
		bytes_left_this_sector -= (packed_deltas[packed_delta_count].length + 4);
		frame_time += ROUTINE_TSTATE_COUNT + (packed_deltas[packed_delta_count].length * INI_TSTATE_COUNT);
		if (bytes_left_this_sector == 0) bytes_left_this_sector = 512;
		packed_delta_count++;
	}
	
	*frame_time_out = frame_time;
	*final_delta_count_out = packed_delta_count;
}

static int max_delta_length_candidates[18] = {4,6,8,10,12,14,16,20,24,28,32,36,40,48,52,56,60,64};

void delta_compressor_compute_optimal_deltas(unsigned char *old_frame, unsigned char *new_frame, st_delta *packed_deltas, int *final_delta_count_out) {
	int delta_count, final_delta_count, max_delta_length;
	double frame_time;
	
	static st_delta deltas[10000];
	int i;
	
	for (i = 0; i < 18; i++) {
		max_delta_length = max_delta_length_candidates[i];
		delta_count = delta_compressor_compute_deltas(old_frame, new_frame, deltas, max_delta_length);
		delta_compressor_pack_deltas(deltas, delta_count, packed_deltas, &frame_time, &final_delta_count);
		if (frame_time <= MAX_FRAME_TIME) break;
	}
	*final_delta_count_out = final_delta_count;
	
	printf("delta len: %d\tpackets: %d\taudio: %dHz\tframe time: %f\n", max_delta_length, final_delta_count, final_delta_count*25, frame_time);
	if (frame_time > MAX_FRAME_TIME) {
		printf("WARNING: too much video data for one frame - playback will probably be jittery\n");
	}
}
