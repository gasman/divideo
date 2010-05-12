require 'video_reader'
require 'image_to_spectrum'
require 'delta_compressor'
require 'divideo_converter'
require 'image_preprocessor'

module Divideo
	REAL_FRAME_TIME = 141816 # real length of a frame; used for audio timings
	
	class VideoWriter
		attr_accessor :zx_converter
		attr_accessor :max_frames
		attr_accessor :contrast, :brightlevel, :volume_boost
		
		def initialize(filename, opts = {})
			@filename = filename
			@internal_filename = opts[:name] || File.basename(@filename)
			@outfile = File.open(filename, 'wb')
			@prev_frame = "\x00" * 0x1b00
			@prev_prev_frame = "\x00" * 0x1b00
			@prev_frame_pack_data = nil
			@max_frames = opts[:max_frames]
			@volume_boost = opts[:volume_boost] || 1
			@contrast = opts[:contrast]
			@brightlevel = opts[:brightlevel]
			
			@border_colour = opts[:border_colour] || 0
			@zx_converter = ImageToSpectrum::Converter.new(opts[:use_colour] || false, opts[:dither_level] || 2)
		end
		
		def <<(video)
			frame_number = 0
			loop do
				frame_ppm = video.read_ppm
				frame_number += 1
				break if frame_ppm.nil? or (@max_frames and frame_number > @max_frames)
				frame_bitmap = ImagePreprocessor.ppm_to_bitmap(frame_ppm, @contrast, @brightlevel)
				frame = @zx_converter.convert(frame_bitmap)
				
				print "frame #{frame_number}\t"
				deltas = DeltaCompressor.compute_optimal_deltas(@prev_prev_frame, frame)
				current_frame_pack_data = DivideoConverter.encode_deltas(deltas, frame, frame_number * REAL_FRAME_TIME, video, @volume_boost)
				
				current_frame_sector_size = ((current_frame_pack_data.length + 3) / 512.0).ceil
				
				if @prev_frame_pack_data.nil? # this is the first frame; output the header
					header = "DivIDEog\x01"
					header << [current_frame_sector_size, @border_colour].pack('CC')
					header << "\x00"*21 # reserved
					header << @internal_filename
					header << ("\x00" * ( -header.length % 512) )
					@outfile << header
				else
					@prev_frame_pack_data << [current_frame_sector_size, 0, 0, 0].pack('CCCC')
					# pad to 512b
					@prev_frame_pack_data << ("\x00" * ( -@prev_frame_pack_data.length % 512) )
					@outfile << @prev_frame_pack_data
				end
				
				@prev_frame_pack_data = current_frame_pack_data
				@prev_prev_frame = @prev_frame
				@prev_frame = frame
			end
		end
		
		def close
			@outfile << @prev_frame_pack_data << [0, 0, 0, 0].pack('CCCC')
			@outfile.close
		end
	end
end
