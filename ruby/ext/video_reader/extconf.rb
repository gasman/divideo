# --with-ffmpeg-dir=/opt/ffmpeg

require 'mkmf'

ffmpeg_include, ffmpeg_lib = dir_config("ffmpeg")

$CFLAGS << " -W -Wall"
#$LDFLAGS << " -rpath #{ffmpeg_lib}"

if have_library("avformat") and find_header('libavformat/avformat.h') and
		have_library("avcodec") and find_header('libavcodec/avcodec.h') and
		have_library("swscale") and find_header('libswscale/swscale.h')
	create_makefile("video_reader")
else
	STDERR.puts "missing library"
	exit 1
end
