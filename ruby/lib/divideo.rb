require 'video_reader'
require 'rmagick'
require 'image_to_spectrum'
require 'delta_compressor'

module Divideo
	REAL_FRAME_TIME = 141816 # real length of a frame; used for audio timings
	
	def self.resize_for_spectrum(img, contrast, brightlevel)
		img.resize_to_fit!(256,192)
		img.background_color = 'black'
		img = img.extent(256,192,128 - (img.columns / 2), 96 - (img.rows / 2))
		if contrast or brightlevel
			img = img.sigmoidal_contrast_channel(contrast || 5.5, Magick::QuantumRange * (brightlevel || 0.30), true)
		end
		return img
	end
	
	def self.audio_level_to_ay_level(v, volume_boost = 1)
		v *= volume_boost
		if v < 225 then 0
		elsif v < 560 then 1
		elsif v < 811 then 2
		elsif v < 1169 then 3
		elsif v < 1706 then 4
		elsif v < 2401 then 5
		elsif v < 3631 then 6
		elsif v < 5014 then 7
		elsif v < 7107 then 8
		elsif v < 10114 then 9
		elsif v < 13150 then 10
		elsif v < 16716 then 11
		elsif v < 20605 then 12
		elsif v < 25156 then 13
		elsif v < 30279 then 14
		else 15
		end
	end
	
	def self.encode_deltas(deltas, frame, frame_start_time, video, volume_boost = 1)
		output = ''
		deltas.each_with_index do |delta, i|
			if deltas[i+1]
				original_audio_level = video.average_audio_level(frame_start_time + deltas[i+1][:time])
			else
				original_audio_level = video.average_audio_level(frame_start_time + REAL_FRAME_TIME)
			end
	#		original_audio_level = 1
			audio_level = audio_level_to_ay_level(original_audio_level, volume_boost)
			data = frame[delta[:offset] ... (delta[:offset] + delta[:length])]
			packet_data = [0xc000 + delta[:offset], 254-(delta[:length] * 2), 0x80 + audio_level].pack('vCC') + data
			output << packet_data
		end
		
		return output
	end
	
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
				img = Magick::Image.from_blob(frame_ppm)[0]
				img = Divideo.resize_for_spectrum(img, @contrast, @brightlevel)
				frame = @zx_converter.convert(img.export_pixels_to_str(0, 0, 256, 192, "RGBP"))
				
				print "frame #{frame_number}\t"
				deltas = DeltaCompressor.compute_optimal_deltas(@prev_prev_frame, frame)
				current_frame_pack_data = Divideo.encode_deltas(deltas, frame, frame_number * REAL_FRAME_TIME, video, @volume_boost)
				
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
