#include "ruby.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "image_to_spectrum.h"

static unsigned char zx_palette[2][8][4] = {
	{"\x00\x00\x00\x00", "\x00\x00\xc0\x00", "\xc0\x00\x00\x00", "\xc0\x00\xc0\x00",
	"\x00\xc0\x00\x00", "\x00\xc0\xc0\x00", "\xc0\xc0\x00\x00", "\xc0\xc0\xc0\x00"},

	{"\x00\x00\x00\x00", "\x00\x00\xff\x00", "\xff\x00\x00\x00", "\xff\x00\xff\x00",
	"\x00\xff\x00\x00", "\x00\xff\xff\x00", "\xff\xff\x00\x00", "\xff\xff\xff\x00"}
};

static VALUE converter_convert(VALUE self, VALUE image_data_val) {
	st_converter_data *convdata;
	Data_Get_Struct(self, st_converter_data, convdata);
	
	unsigned char *image_data;
	image_data = (unsigned char *)StringValuePtr(image_data_val);
	
	static unsigned char zx_data[0x1b00];
	
	image_to_spectrum_convert(convdata, image_data, zx_data);
	
	return rb_str_new((char *)zx_data, 0x1b00);
}

#define PIXEL_DATA_SIZE 256*192*4
static VALUE render_as_pixels(VALUE obj, VALUE zx_data_val) {
	char *zx_data;
	static char pixel_data[PIXEL_DATA_SIZE];
	char *ptr;
	int third, pixrow, charrow, x;
	unsigned char screen_byte, attr_byte, bit;
	unsigned char *ink, *paper;
	
	zx_data = StringValuePtr(zx_data_val);
	ptr = pixel_data;
	
	for (third = 0; third < 0x1800; third += 0x0800) {
		for (charrow = 0; charrow < 0x0100; charrow += 0x0020) {
			for (pixrow = 0; pixrow < 0x0800; pixrow += 0x0100) {
				for (x = 0; x < 0x20; x++) {
					screen_byte = zx_data[third | pixrow | charrow | x];
					attr_byte = zx_data[0x1800 | (third >> 3) | charrow | x];
					ink = zx_palette[(attr_byte & 0x40) >> 6][attr_byte & 0x07];
					paper = zx_palette[(attr_byte & 0x40) >> 6][(attr_byte & 0x38) >> 3];
					for (bit = 0x80; bit > 0; bit >>= 1) {
						if (screen_byte & bit) {
							memcpy(ptr, ink, 4);
						} else {
							memcpy(ptr, paper, 4);
						}
						ptr += 4;
					}
				}
			}
		}
	}
	
	return rb_str_new(pixel_data, PIXEL_DATA_SIZE);
}

static VALUE converter_allocate (VALUE klass) {
	st_converter_data *convdata;
	VALUE rb_instance;
	
	rb_instance = Data_Make_Struct(klass, st_converter_data, 0, 0, convdata);
	return rb_instance;
}

static VALUE converter_initialize(VALUE self, VALUE use_colour, VALUE rb_dither_map_size) {
	st_converter_data *convdata;
	Data_Get_Struct(self, st_converter_data, convdata);
	
	image_to_spectrum_converter_init(convdata, !!use_colour, NUM2INT(rb_dither_map_size));
	
	return self;
}

void Init_image_to_spectrum() {
	VALUE rbm_image_to_spectrum;
	VALUE rbc_converter;
	
	rbm_image_to_spectrum = rb_define_module("ImageToSpectrum");
	rbc_converter = rb_define_class_under(rbm_image_to_spectrum, "Converter", rb_cObject);
	rb_define_alloc_func (rbc_converter, converter_allocate);
	rb_define_method(rbc_converter, "initialize", converter_initialize, 2);
	rb_define_method(rbc_converter, "convert", converter_convert, 1);
	
	rb_define_module_function(rbm_image_to_spectrum, "render_as_pixels", render_as_pixels, 1);
}
