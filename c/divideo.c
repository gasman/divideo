#include "divideo_converter.h"

int main(int argc, char *argv[]) {
	video_writer vwriter;
	
	int use_colour = 0;
	int dither_map_size = 2;
	
	video_writer_init();
	
	video_writer_create(&vwriter, argv[2], "test video", use_colour, dither_map_size);
	vwriter.border_colour = 0;
	video_writer_append(&vwriter, argv[1]);
	video_writer_close(&vwriter);
	video_writer_destroy(&vwriter);
	
	return 0;
}
