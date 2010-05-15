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

static VALUE video_writer_allocate_rb (VALUE klass) {
	video_writer *vwriter;
	VALUE rb_instance;
	
	rb_instance = Data_Make_Struct(klass, video_writer, 0, video_writer_destroy, vwriter);
	return rb_instance;
}

static VALUE video_writer_initialize_rb(VALUE self, VALUE filename_rb, VALUE opts) {
	video_writer *vwriter;
	Data_Get_Struct(self, video_writer, vwriter);
	
	char *filename = StringValuePtr(filename_rb);
	
	char *internal_filename = "test video";
	int use_colour = 0;
	int dither_map_size = 2;
	
	video_writer_create(vwriter, filename, internal_filename, use_colour, dither_map_size);
	
	return self;
}

static VALUE video_writer_append_rb(VALUE self, VALUE filename_rb) {
	video_writer *vwriter;
	Data_Get_Struct(self, video_writer, vwriter);
	
	char *filename = StringValuePtr(filename_rb);
	video_writer_append(vwriter, filename);
	return self;
}

static VALUE video_writer_close_rb(VALUE self) {
	video_writer *vwriter;
	Data_Get_Struct(self, video_writer, vwriter);
	
	video_writer_close(vwriter);
	return self;
}

void Init_divideo_converter() {
	VALUE rbm_divideo_converter;
	
	rbm_divideo_converter = rb_define_module("DivideoConverter");
	rb_define_module_function(rbm_divideo_converter, "encode_deltas", encode_deltas_ruby, 5);
	
	video_writer_init();
	VALUE rbc_video_writer = rb_define_class("VideoWriter", rb_cObject);
	rb_define_alloc_func (rbc_video_writer, video_writer_allocate_rb);
	rb_define_method(rbc_video_writer, "initialize", video_writer_initialize_rb, 2);
	rb_define_method(rbc_video_writer, "<<", video_writer_append_rb, 1);
	rb_define_method(rbc_video_writer, "close", video_writer_close_rb, 0);
}
