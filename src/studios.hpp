#pragma once

#include <iostream>

#include "mkvwriter/MatroskaMuxer.h"
#include "audio.hpp"
#include "video.hpp"

class Studios
{
public:
	Studios(int argc, char **argv)
	{
		mkvwriter::MatroskaMuxer muxer;
		if (argc < 2)
			std::cerr << "Error: Insufficient arguments provided" << std::endl <<
				"Usage: muxer <output_filename: file.mkv>/* <window mode: large || small>*/" << std::endl;

		std::string outfile = argv[1];
		if (muxer.Set_OutputFilename(outfile))
			std::cout << "-> Audio error: failed setting output filename" << std::endl;

		int stream = 0;
		float V_fps = 20;

		auto A = Audio(&muxer, stream, V_fps);
		
		stream++;
		auto V = Video(&muxer, stream, /*argv, */V_fps);

		long double A_x = 0;
		long double A_x_freq = 0;
		double A_freq = 440;
		double A_w_freq = 0.5;

		uint64_t total_frame_count = 120;
		uint64_t frame_count = 0;
		uint64_t written_frame_count = 0;

		std::cout << "loop begins" << std::endl;
		while (frame_count < total_frame_count) {
			A.encode_audio_frame(A_x, A_x_freq, A_freq, A_w_freq);
			V.encode_video_frame(frame_count, written_frame_count);
			frame_count++;
		}
		std::cout << std::endl;
		A.destroy_audio_stream();

		//V.close_window();
		V.flush_frames(written_frame_count);
		V.destroy_video_stream();

		std::cout << "Read " << written_frame_count << " frames\n";

		muxer.CloseFile();
	}
};