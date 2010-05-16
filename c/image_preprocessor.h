#ifndef IMAGE_PREPROCESSOR_H
#define IMAGE_PREPROCESSOR_H

#include <stddef.h>

void image_preprocessor_init();
void image_preprocessor_destroy();
size_t image_preprocessor_ppm_to_bitmap(char *frame_ppm, size_t frame_ppm_len, double contrast, double brightlevel, char *bitmap);

#endif /* IMAGE_PREPROCESSOR_H */
