#include "video_reader.h"

#include <stdlib.h>
#include <stdio.h>

#include "ruby.h"

VALUE rb_eUnsupportedFormat;
VALUE rb_video_reader;

static void
video_reader_free_rb (st_video_reader_data *vrdata)
{
	video_reader_close_data(vrdata);
}

static VALUE
video_reader_allocate (VALUE klass)
{
	st_video_reader_data *video_reader_data;
	VALUE rb_instance;
	
	rb_instance = Data_Make_Struct(klass, st_video_reader_data, 0, video_reader_free_rb, video_reader_data);
	video_reader_init_data(video_reader_data);
	return rb_instance;
}

static VALUE
video_reader_initialize_rb(VALUE self, VALUE filename)
{
	st_video_reader_data *vrdata;
	Data_Get_Struct(self, st_video_reader_data, vrdata);
	
	if (Qfalse == rb_funcall(rb_cFile, rb_intern("file?"), 1, filename))
		rb_raise(rb_eArgError,
			"Input file not found: %s",
			StringValuePtr(filename));
	
	int error = video_reader_open(vrdata, StringValuePtr(filename));
	
	if (error < 0) {
		rb_raise(rb_eUnsupportedFormat,
			"ffmpeg failed to open input file %s", 
			StringValuePtr(filename));
	}
	
	return self;
}

static VALUE
video_reader_read_ppm_rb(VALUE self) //, VALUE r_width, VALUE r_height)
{
	st_video_reader_data *vrdata;
	Data_Get_Struct(self, st_video_reader_data, vrdata);
	int size;
	
	char *data_string = video_reader_read_ppm(vrdata, &size);
	
	if (data_string == NULL) {
		return Qnil;
	} else {
		return rb_str_new(data_string, size);
	}
}

/* compute the average audio level from the previous position to this one (given in tstates) */
static VALUE
video_reader_average_audio_level_rb(VALUE self, VALUE rb_tstate)
{
	st_video_reader_data *vrdata;
	Data_Get_Struct(self, st_video_reader_data, vrdata);
	
	long long tstate = NUM2LL(rb_tstate);
	
	return LL2NUM( video_reader_average_audio_level(vrdata, tstate) );
}

void Init_video_reader() {
	video_reader_init();
	
	rb_video_reader = rb_define_class("VideoReader", rb_cObject);
	rb_define_alloc_func (rb_video_reader, video_reader_allocate);

	rb_define_method(rb_video_reader, "initialize", video_reader_initialize_rb, 1);
	rb_define_method(rb_video_reader, "read_ppm", video_reader_read_ppm_rb, 0);
	rb_define_method(rb_video_reader, "average_audio_level", video_reader_average_audio_level_rb, 1);
	rb_eUnsupportedFormat = rb_define_class_under(rb_video_reader, "UnsupportedFormat", rb_eStandardError);
}
