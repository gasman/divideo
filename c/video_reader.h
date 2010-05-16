#ifndef VIDEO_READER_H
#define VIDEO_READER_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

typedef struct av_frame_list {
	AVFrame *frame;
	struct av_frame_list *next;
} st_av_frame_list;

typedef struct audio_buffer {
	uint8_t buffer[AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	uint8_t *buffer_end;
	struct audio_buffer *next;
} st_audio_buffer;

typedef struct video_reader_data {
	AVFormatContext *format_context;
	int video_stream_index;
	int audio_stream_index;
	AVCodecContext *audio_codec_context;
	AVCodecContext *video_codec_context;
	st_audio_buffer *first_audio_buffer;
	st_audio_buffer *last_audio_buffer;
	uint8_t *audio_buffer_position;
	
	long long audio_sample_number;
	AVFrame *incomplete_video_frame;
	st_av_frame_list *video_frame_list_start;
	st_av_frame_list *video_frame_list_end;
} st_video_reader_data;

void video_reader_init();
void video_reader_init_data(st_video_reader_data *vrdata);
int video_reader_open(st_video_reader_data *vrdata, char *filename);
char *video_reader_read_ppm(st_video_reader_data *vrdata, int *size_out);
long long video_reader_average_audio_level(st_video_reader_data *vrdata, long long tstate);
void video_reader_close_data(st_video_reader_data *vrdata);

#endif