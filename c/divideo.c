#include "divideo_converter.h"
#include <argtable2.h>
#include <stdio.h>

int main(int argc, char **argv) {
	
	struct arg_int *border_opt = arg_int0("b", "border", NULL, "Set border colour (0-7)");
	struct arg_lit *colour_opt = arg_lit0("c", "colour", "Perform conversion in colour");
	struct arg_int *dither_opt = arg_int0("d", "dither", NULL, "Set dither size (1 = none, 2 = 2x2, 4 = 4x4)");
	struct arg_int *frames_opt = arg_int0("f", "frames", "<N>", "Only convert the first N frames");
	struct arg_dbl *contrast_opt = arg_dbl0("k", "contrast", NULL, "Set contrast (5.5 works well)");
	struct arg_dbl *brightlevel_opt = arg_dbl0("l", "brightlevel", NULL, "Set midpoint brightness level (0.3 works well)");
	struct arg_lit *mono_opt = arg_lit0("m", "mono", "Perform conversion in black and white");
	struct arg_str *name_opt = arg_str0("n", "name", NULL, "Specify internal filename for video");
	struct arg_dbl *volume_opt = arg_dbl0("v", "volume", "<N>", "Multiply volume level by N");
	struct arg_lit *help_opt = arg_lit0("h", "help", "Print this help and exit");
	
	struct arg_file *files_opt = arg_filen(NULL, NULL, "", 2, 2, NULL);
	struct arg_rem *dest_opt = arg_rem ("INFILE OUTFILE", NULL);
	
	struct arg_end *end_opts = arg_end(20);
	
	void *argtable[] = {border_opt, colour_opt, dither_opt, frames_opt, contrast_opt,
		brightlevel_opt, mono_opt, name_opt, volume_opt, help_opt, files_opt, dest_opt, end_opts};
	
	if (arg_nullcheck(argtable) != 0) {
		printf("error: insufficient memory\n");
		return -1;
	}
	
	/* set defaults */
	border_opt->ival[0] = 0;
	dither_opt->ival[0] = 2;
	frames_opt->ival[0] = -1;
	contrast_opt->dval[0] = -1;
	brightlevel_opt->dval[0] = -1;
	volume_opt->dval[0] = 1;
	
	int nerrors = arg_parse(argc,argv,argtable);

	/* special case: '--help' takes precedence over error reporting */
	if (help_opt->count > 0)
		{
		printf("Usage: %s", "divideo");
		arg_print_syntax(stdout,argtable,"\n");
		arg_print_glossary(stdout,argtable,"  %-25s %s\n");
		return 0;
	}
	
	if (nerrors > 0) {
		arg_print_errors(stdout, end_opts, "divideo");
		printf("Usage: %s", "divideo");
		arg_print_syntax(stdout,argtable,"\n");
		return -1;
	}
	
	video_writer vwriter;
	
	video_writer_init();
	
	const char *input_file = files_opt->filename[0];
	const char *output_file = files_opt->filename[1];
	
	const char *internal_name;
	if (name_opt->count) {
		internal_name = name_opt->sval[0];
	} else {
		internal_name = files_opt->basename[0];
	}
	
	int use_colour = (colour_opt->count >= mono_opt->count);
	
	video_writer_create(&vwriter, output_file, internal_name, use_colour, dither_opt->ival[0]);
	vwriter.border_colour = border_opt->ival[0];
	vwriter.max_frames = frames_opt->ival[0];
	vwriter.contrast = contrast_opt->dval[0];
	vwriter.brightlevel = brightlevel_opt->dval[0];
	vwriter.volume_boost = volume_opt->dval[0];
	
	video_writer_append(&vwriter, input_file);
	video_writer_close(&vwriter);
	video_writer_destroy(&vwriter);
	
	return 0;
}
