#include "ruby.h"
#include "image_preprocessor.h"

static VALUE ppm_to_bitmap_ruby(VALUE self, VALUE frame_ppm_rb, VALUE contrast_rb, VALUE brightlevel_rb) {
	char *frame_ppm = StringValuePtr(frame_ppm_rb);
	size_t frame_ppm_len = RSTRING_LEN(frame_ppm_rb);
	
	double contrast, brightlevel;
	
	if (contrast_rb == Qnil) {
		contrast = -1;
	} else {
		contrast = NUM2DBL(contrast_rb);
	}
	if (brightlevel_rb == Qnil) {
		brightlevel = -1;
	} else {
		brightlevel = NUM2DBL(brightlevel_rb);
	}
	
	char bitmap[200000];
	
	int bitmap_len = image_preprocessor_ppm_to_bitmap(frame_ppm, frame_ppm_len, contrast, brightlevel, bitmap);
	
	return rb_str_new(bitmap, bitmap_len);
}

void Init_image_preprocessor() {
	VALUE rbm_image_preprocessor = rb_define_module("ImagePreprocessor");
	
	image_preprocessor_init();
	rb_define_module_function(rbm_image_preprocessor, "ppm_to_bitmap", ppm_to_bitmap_ruby, 3);
}
