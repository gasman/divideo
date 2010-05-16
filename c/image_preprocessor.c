#include "image_preprocessor.h"
#include <magick/MagickCore.h>

void image_preprocessor_init() {
	MagickCoreGenesis("ImagePreprocessor", MagickFalse);
}

void image_preprocessor_destroy() {
	MagickCoreTerminus();
}

size_t image_preprocessor_ppm_to_bitmap(char *frame_ppm, size_t frame_ppm_len, double contrast, double brightlevel, char *bitmap) {
	ImageInfo *image_info = CloneImageInfo(NULL);
	ExceptionInfo exception;
	
	GetExceptionInfo(&exception);
	Image *img = BlobToImage(image_info, frame_ppm, frame_ppm_len, &exception);
	DestroyExceptionInfo(&exception);
	
	/* resize to fit within 256*192 */
	unsigned long resized_height = (img->rows * 256) / img->columns;
	unsigned long resized_width = (img->columns * 192) / img->rows;
	if (resized_height <= 192) {
		resized_width = 256;
	} else {
		resized_height = 192;
	}
	
	GetExceptionInfo(&exception);
	Image *resized_img = AdaptiveResizeImage(img, resized_width, resized_height, &exception);
	DestroyExceptionInfo(&exception);
	DestroyImage(img);
	
	/* set extent to 256*192, with image centered */
	RectangleInfo extent_rect;
	extent_rect.width = 256;
	extent_rect.height = 192;
	extent_rect.x = 128 - (resized_width / 2);
	extent_rect.y = 96 - (resized_height / 2);
	
	/* set background black */
	GetExceptionInfo(&exception);
	PixelPacket black;
	QueryColorDatabase("black", &black, &exception);
	DestroyExceptionInfo(&exception);
	resized_img->background_color = black;
	
	GetExceptionInfo(&exception);
	Image *extended_img = ExtentImage(resized_img, &extent_rect, &exception);
	DestroyExceptionInfo(&exception);
	DestroyImage(resized_img);
	
	if (contrast >= 0 || brightlevel >= 0) {
		/* fill in default contrast / bright levels */
		if (contrast < 0) contrast = 5.5;
		if (brightlevel < 0) brightlevel = 0.30;
		SigmoidalContrastImageChannel(extended_img, AllChannels, MagickTrue, contrast, QuantumRange * brightlevel);
	}
	/* TODO: apply brightness/contrast */
	
	GetExceptionInfo(&exception);
	ExportImagePixels(extended_img, 0, 0, 256, 192, "RGBP", CharPixel, bitmap, &exception);
	DestroyExceptionInfo(&exception);
	DestroyImage(extended_img);
	
	DestroyImageInfo(image_info);
	
	return 256*192*4;
}
