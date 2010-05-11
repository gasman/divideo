#include "ruby.h"
#include "../delta_compressor/delta_compressor.h"
#include "../video_reader/video_reader.h"
#include "divideo_converter.h"

static VALUE encode_deltas_ruby(VALUE self, VALUE deltas_rb, VALUE frame_rb, VALUE frame_start_time_rb, VALUE video_rb, VALUE volume_boost_rb) {
	
	st_delta deltas[10000];
	
	int delta_count = RARRAY_LEN(deltas_rb);
	int i;
	
	/* populate deltas array from the Ruby one */
	for (i = 0; i < delta_count; i++) {
		
		VALUE delta_rb = RARRAY_PTR(deltas_rb)[i];
		
		VALUE offset_rb = rb_hash_aref( delta_rb, ID2SYM(rb_intern("offset")) );
		deltas[i].offset = FIX2INT(offset_rb);

		VALUE length_rb = rb_hash_aref( delta_rb, ID2SYM(rb_intern("length")) );
		deltas[i].length = FIX2INT(length_rb);
		
		VALUE time_rb = rb_hash_aref( delta_rb, ID2SYM(rb_intern("time")) );
		deltas[i].time = NUM2DBL(time_rb);
	}
	
	char *frame = StringValuePtr(frame_rb);
	
	double frame_start_time = NUM2DBL(frame_start_time_rb);
	
	st_video_reader_data *video;
	Data_Get_Struct(video_rb, st_video_reader_data, video);
	
	double volume_boost = NUM2DBL(volume_boost_rb);
	
	char output[100000];
	
	size_t output_length = divideo_converter_encode_deltas(deltas, delta_count, (unsigned char *)frame, frame_start_time, video, volume_boost, (unsigned char *)output);
	
	return rb_str_new(output, output_length);
	
}

void Init_divideo_converter() {
	VALUE rbm_divideo_converter;
	
	rbm_divideo_converter = rb_define_module("DivideoConverter");
	rb_define_module_function(rbm_divideo_converter, "encode_deltas", encode_deltas_ruby, 5);
}
