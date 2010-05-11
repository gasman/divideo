#include "video_reader.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

static void
begin_video_frame (st_video_reader_data *vrdata)
{
	vrdata->incomplete_video_frame = avcodec_alloc_frame();
}

static void
decode_audio_packet(st_video_reader_data *vrdata, AVPacket *packet)
{
	int out_size, len;
	st_audio_buffer *buffer = malloc(sizeof(st_audio_buffer));
	
	out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	len = avcodec_decode_audio3(vrdata->audio_codec_context, (int16_t *)buffer->buffer, &out_size, packet);
	buffer->buffer_end = buffer->buffer + out_size;
	
	/* put buffer onto the pile */
	buffer->next = NULL;
	if (vrdata->first_audio_buffer == NULL) vrdata->first_audio_buffer = buffer;
	if (vrdata->last_audio_buffer != NULL) vrdata->last_audio_buffer->next = buffer;
	vrdata->last_audio_buffer = buffer;
	
	/*
	int buffer_count = 0;
	st_audio_buffer *p = vrdata->first_audio_buffer;
	while (p != NULL) {
		buffer_count++;
		p = p->next;
	}
	printf("%d audio buffers in queue\n", buffer_count);
	*/
}

static AVFrame *
alloc_picture(int pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;
	
	picture = avcodec_alloc_frame();
	if (!picture)
		return NULL;
	size = avpicture_get_size(pix_fmt, width, height);
	picture_buf = av_malloc(size);
	if (!picture_buf) {
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf,
		pix_fmt, width, height);
	return picture;
}

static void
continue_video_frame (st_video_reader_data *vrdata, AVPacket *packet)
{
	int remaining = packet->size;
	uint8_t *data_buffer = packet->data;
	int decoded;
	int frame_complete = 0;
	
	while(remaining > 0) {
		decoded = avcodec_decode_video(vrdata->video_codec_context, vrdata->incomplete_video_frame,
			&frame_complete, data_buffer, remaining);
		remaining -= decoded;
		// pointer seek forward
		data_buffer += decoded;
	}
	if (frame_complete) {
		/* recode */
		int width = vrdata->video_codec_context->width;
		int height = vrdata->video_codec_context->height;
		
		struct SwsContext *img_convert_ctx = NULL;
		img_convert_ctx = sws_getContext(
			vrdata->video_codec_context->width, vrdata->video_codec_context->height, vrdata->video_codec_context->pix_fmt,
			width, height, PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
		
		AVFrame *rgb24_frame = alloc_picture(PIX_FMT_RGB24, width, height);
		
		sws_scale(img_convert_ctx, vrdata->incomplete_video_frame->data, vrdata->incomplete_video_frame->linesize,
			0, height, rgb24_frame->data, rgb24_frame->linesize);
		
		av_free(img_convert_ctx);
		av_free(vrdata->incomplete_video_frame);
		//printf("Completed video frame %d\n", rgb24_frame);
		
		/* put frame onto the pile */
		st_av_frame_list *frame_list_item = malloc(sizeof(st_av_frame_list));
		frame_list_item->frame = rgb24_frame;
		frame_list_item->next = NULL;
		if (vrdata->video_frame_list_start == NULL) vrdata->video_frame_list_start = frame_list_item;
		if (vrdata->video_frame_list_end != NULL) vrdata->video_frame_list_end->next = frame_list_item;
		vrdata->video_frame_list_end = frame_list_item;
		
		/*
		int frame_count = 0;
		st_av_frame_list *p = vrdata->video_frame_list_start;
		while (p != NULL) {
			frame_count++;
			p = p->next;
		}
		printf("%d video frames in queue\n", frame_count);
		*/
		
		begin_video_frame(vrdata);
	}
}

static int
get_packet (st_video_reader_data *vrdata)
{
	AVPacket packet;
	if (av_read_frame(vrdata->format_context, &packet) >= 0) {
		if ( packet.stream_index == vrdata->audio_stream_index ) {
			decode_audio_packet(vrdata, &packet);
		} else if ( packet.stream_index == vrdata->video_stream_index ) {
			continue_video_frame(vrdata, &packet);
		}
		av_free_packet(&packet);
		return 1;
	} else {
		return 0;
	}
}

static AVFrame *
next_video_frame (st_video_reader_data *vrdata)
{
	AVFrame *frame;
	st_av_frame_list *frame_list_entry;
	
	/* check whether we need to read more packets before we have a complete frame */
	while (vrdata->video_frame_list_start == NULL) {
		if (get_packet(vrdata) == 0) break;
	}
	
	if (vrdata->video_frame_list_start == NULL) {
		return NULL;
	} else {
		frame_list_entry = vrdata->video_frame_list_start;
		frame = frame_list_entry->frame;
		//printf("Retrieved video frame %d\n", frame);
		vrdata->video_frame_list_start = frame_list_entry->next;
		if (frame_list_entry->next == NULL) vrdata->video_frame_list_end = NULL;
		free(frame_list_entry);
		return frame;
	}
}

static st_audio_buffer *
next_audio_buffer(st_video_reader_data *vrdata)
{
	st_audio_buffer *buffer;
	
	/* advance/free the last one, if any */
	buffer = vrdata->first_audio_buffer;
	if (buffer) {
		vrdata->first_audio_buffer = buffer->next;
		if (buffer->next == NULL) vrdata->last_audio_buffer = NULL;
		free(buffer);
	}
	
	/* check whether we need to read more packets before we have a complete frame */
	while (vrdata->first_audio_buffer == NULL) {
		if (get_packet(vrdata) == 0) break;
	}
	vrdata->audio_buffer_position = vrdata->first_audio_buffer->buffer;
	return vrdata->first_audio_buffer;
}

void video_reader_init() {
	av_register_all();
}

void video_reader_init_data(st_video_reader_data *vrdata) {
	vrdata->format_context = NULL;
}

int video_reader_open(st_video_reader_data *vrdata, char *filename) {
	AVFormatContext *format_context;
	unsigned int stream_index;
	
	AVCodec *audio_codec, *video_codec;
	AVCodecContext *audio_codec_context, *video_codec_context;
	
	/* TODO: check that file-not-found fails gracefully with an error response */
	int error = av_open_input_file(&format_context, filename, NULL, 0, NULL);
	
	if (error < 0) {
		fprintf(stderr, "ffmpeg failed to open input file %s\n", filename);
		return error;
	}
	
	vrdata->format_context = format_context;
	
	// Retrieve stream information
	error = av_find_stream_info(vrdata->format_context);
	if (error < 0) {
		fprintf(stderr, "Could not find stream information in file %s\n", filename);
		return error;
	}
	
	/* search for video and audio streams */
	vrdata->audio_stream_index = -1;
	vrdata->video_stream_index = -1;
	
	for(stream_index = 0; stream_index < vrdata->format_context->nb_streams; stream_index++) {
		if (vrdata->format_context->streams[stream_index]->codec->codec_type==CODEC_TYPE_AUDIO && vrdata->audio_stream_index == -1) {
			audio_codec_context = vrdata->format_context->streams[stream_index]->codec;
			audio_codec = avcodec_find_decoder(audio_codec_context->codec_id);
			
			if(!audio_codec) {
				fprintf(stderr, "Warning: unsupported audio codec\n");
			} else {
				avcodec_open(audio_codec_context, audio_codec);
				vrdata->audio_stream_index = stream_index;
				vrdata->audio_codec_context = audio_codec_context;
				vrdata->audio_sample_number = 0;
				vrdata->first_audio_buffer = NULL;
				vrdata->last_audio_buffer = NULL;
				vrdata->audio_buffer_position = NULL;
			}
		} else if (vrdata->format_context->streams[stream_index]->codec->codec_type==CODEC_TYPE_VIDEO && vrdata->video_stream_index == -1) {
			video_codec_context = vrdata->format_context->streams[stream_index]->codec;
			video_codec = avcodec_find_decoder(video_codec_context->codec_id);
			
			if(!video_codec) {
				fprintf(stderr, "Warning: unsupported video codec\n");
			} else {
				avcodec_open(video_codec_context, video_codec);
				vrdata->video_stream_index = stream_index;
				vrdata->video_codec_context = video_codec_context;
				vrdata->video_frame_list_start = NULL;
				vrdata->video_frame_list_end = NULL;
				begin_video_frame(vrdata);
			}
		}
	}
	
	return 0;
}

char *video_reader_read_ppm(st_video_reader_data *vrdata, int *size_out) {
	AVFrame *frame;
	
	if ( (frame = next_video_frame(vrdata)) == NULL) {
		return NULL;
	} else {
		int width = vrdata->video_codec_context->width;
		int height = vrdata->video_codec_context->height;
		
		char header[255];
		sprintf(header, "P6\n%d %d\n255\n", width, height);
		
		int size = strlen(header) + frame->linesize[0] * height;
		char * data_string = malloc(size);
		strcpy(data_string, header);
		
		memcpy(data_string + strlen(header), frame->data[0], frame->linesize[0] * height);
		
		av_free(frame->data[0]);
		av_free(frame);
		
		*size_out = size;
		return data_string;
	}
}

/* compute the average audio level from the previous position to this one (given in tstates) */
long long video_reader_average_audio_level(st_video_reader_data *vrdata, long long tstate) {
	long long end_sample_number = tstate * vrdata->audio_codec_context->sample_rate / 3546900;
	
	int chan;
	
	int sample_count = 0;
	long long sample_total = 0;

	/* ensure we have data; bail out if reading more fails */
	if (vrdata->audio_buffer_position == NULL || vrdata->audio_buffer_position >= vrdata->first_audio_buffer->buffer_end) {
		if (next_audio_buffer(vrdata) == NULL) return 0;
	}
	/* read sample */
	for (chan = 0; chan < vrdata->audio_codec_context->channels; chan++) {
		sample_total += ((short *)vrdata->audio_buffer_position)[chan];
		sample_count++;
	}
	
	/* now advance and read subsequent bytes, if end sample number is strictly higher */
	while (vrdata->audio_sample_number < end_sample_number) {
		vrdata->audio_buffer_position += vrdata->audio_codec_context->channels * sizeof(short);
		vrdata->audio_sample_number += 1;
		
		/* ensure we have data; bail out if reading more fails */
		if (vrdata->audio_buffer_position == NULL || vrdata->audio_buffer_position >= vrdata->first_audio_buffer->buffer_end) {
			if (next_audio_buffer(vrdata) == NULL) break;
		}
		
		/* read sample */
		for (chan = 0; chan < vrdata->audio_codec_context->channels; chan++) {
			sample_total += ((short *)vrdata->audio_buffer_position)[chan];
			sample_count++;
		}
	}
	
	return sample_total / sample_count;
}

void video_reader_close_data(st_video_reader_data *vrdata) {
	if (vrdata->format_context) {
		av_close_input_file( vrdata->format_context );
	}
}
