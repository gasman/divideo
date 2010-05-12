# --with-ffmpeg-dir=/opt/ffmpeg

require 'mkmf'

if find_executable('MagickCore-config')
	$CFLAGS << ' ' + `MagickCore-config --cflags`.strip
	$LDFLAGS << ' ' + `MagickCore-config --ldflags`.strip + ' ' + `MagickCore-config --libs`.strip
else
	STDERR.puts "MagickCore-config not found"
	exit 1
end

create_makefile('image_preprocessor')
