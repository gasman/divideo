#include "ruby.h"
#include "delta_compressor.h"

static VALUE compute_deltas(VALUE self, VALUE old_frame_val, VALUE new_frame_val, VALUE max_delta_length_val) {
	int delta_count;
	
	unsigned char *old_frame = (unsigned char *)StringValuePtr(old_frame_val);
	unsigned char *new_frame = (unsigned char *)StringValuePtr(new_frame_val);
	int max_delta_length = FIX2INT(max_delta_length_val);
	
	static st_delta deltas[10000];
	
	delta_count = delta_compressor_compute_deltas(old_frame, new_frame, deltas, max_delta_length);
	
	VALUE arr_deltas = rb_ary_new2(delta_count);
	VALUE delta_hash;
	
	int i;
	
	for (i = 0; i < delta_count; i++) {
		delta_hash = rb_hash_new();
		
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("offset")), INT2FIX(deltas[i].offset));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("length")), INT2FIX(deltas[i].length));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("time")), rb_float_new(deltas[i].time));
		rb_ary_push(arr_deltas, delta_hash);
	}
	
	return arr_deltas;
}

static VALUE compute_packed_deltas(VALUE self, VALUE old_frame_val, VALUE new_frame_val, VALUE max_delta_length_val) {
	int delta_count, final_delta_count;
	double frame_time;
	
	unsigned char *old_frame = (unsigned char *)StringValuePtr(old_frame_val);
	unsigned char *new_frame = (unsigned char *)StringValuePtr(new_frame_val);
	int max_delta_length = FIX2INT(max_delta_length_val);
	
	static st_delta deltas[10000];
	static st_delta packed_deltas[10000];
	
	delta_count = delta_compressor_compute_deltas(old_frame, new_frame, deltas, max_delta_length);
	delta_compressor_pack_deltas(deltas, delta_count, packed_deltas, &frame_time, &final_delta_count);
	
	VALUE arr_deltas = rb_ary_new2(final_delta_count);
	VALUE delta_hash;
	
	int i;
	
	for (i = 0; i < final_delta_count; i++) {
		delta_hash = rb_hash_new();
		
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("offset")), INT2FIX(packed_deltas[i].offset));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("length")), INT2FIX(packed_deltas[i].length));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("time")), rb_float_new(packed_deltas[i].time));
		rb_ary_push(arr_deltas, delta_hash);
	}
	
	VALUE result = rb_ary_new2(2);
	rb_ary_push(result, rb_float_new(frame_time));
	rb_ary_push(result, arr_deltas);
	
	return result;
}

static VALUE compute_optimal_deltas(VALUE self, VALUE old_frame_val, VALUE new_frame_val) {
	int final_delta_count;
	
	unsigned char *old_frame = (unsigned char *)StringValuePtr(old_frame_val);
	unsigned char *new_frame = (unsigned char *)StringValuePtr(new_frame_val);
	
	static st_delta packed_deltas[10000];
	
	delta_compressor_compute_optimal_deltas(old_frame, new_frame, packed_deltas, &final_delta_count);
	
	VALUE arr_deltas = rb_ary_new2(final_delta_count);
	VALUE delta_hash;
	
	int i;
	
	for (i = 0; i < final_delta_count; i++) {
		delta_hash = rb_hash_new();
		
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("offset")), INT2FIX(packed_deltas[i].offset));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("length")), INT2FIX(packed_deltas[i].length));
		rb_hash_aset(delta_hash, ID2SYM(rb_intern("time")), rb_float_new(packed_deltas[i].time));
		rb_ary_push(arr_deltas, delta_hash);
	}
	
	return arr_deltas;
}

void Init_delta_compressor() {
	VALUE rbm_delta_compressor;
	
	rbm_delta_compressor = rb_define_module("DeltaCompressor");
	rb_define_module_function(rbm_delta_compressor, "compute_optimal_deltas", compute_optimal_deltas, 2);
	rb_define_module_function(rbm_delta_compressor, "compute_packed_deltas", compute_packed_deltas, 3);
	rb_define_module_function(rbm_delta_compressor, "compute_deltas", compute_deltas, 3);
}
