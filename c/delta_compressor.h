#ifndef DELTA_COMPRESSOR_H
#define DELTA_COMPRESSOR_H

#include <stddef.h>

typedef struct delta {
	size_t offset;
	size_t length;
	double time;
} st_delta;

int delta_compressor_compute_deltas(unsigned char *old_frame, unsigned char *new_frame, st_delta *deltas, int max_delta_length);
void delta_compressor_pack_deltas(st_delta *deltas, int delta_count, st_delta *packed_deltas, double *frame_time, int *final_delta_count);
void delta_compressor_compute_optimal_deltas(unsigned char *old_frame, unsigned char *new_frame, st_delta *deltas, int *delta_count);

#endif /* DELTA_COMPRESSOR_H */
