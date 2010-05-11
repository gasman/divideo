#ifndef DIVIDEO_CONVERTER_H
#define DIVIDEO_CONVERTER_H

#include <stddef.h>
#include "../delta_compressor/delta_compressor.h"
#include "../video_reader/video_reader.h"

size_t divideo_converter_encode_deltas(st_delta *deltas, size_t delta_count, unsigned char *frame, double frame_start_time, st_video_reader_data *video, double volume_boost, unsigned char *output);

#endif