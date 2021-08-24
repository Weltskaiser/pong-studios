#pragma once

#include <iostream>

#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp> //
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp> //
#include <SFML/Graphics/Sprite.hpp> //

//#include "mkvwriter/MatroskaMuxer.h"
#include <x264.h>

class Video
{
	mkvwriter::MatroskaMuxer *muxer;

	int width;
	int height;
	sf::RenderWindow window;
	sf::RenderTexture r_texture;
	sf::RectangleShape rect_0;
	sf::RectangleShape rect_1;
	sf::RectangleShape rect_2;
	sf::RectangleShape rect_3;
	int luma_size;

	std::string codecId;
	int stream;
	x264_picture_t pic;
	x264_picture_t pic_out;
	x264_t *h;
	x264_nal_t *nal;
	int i_nal;

	float fps;

	void SetStreamHeader(void)
	{
		std::cout << "Writing " << codecId << " to track " << stream+1 << std::endl;

		if (muxer->Set_Track_CodecID(stream+1, codecId))
			std::cout << "-> Video error: failed setting track codec ID" << std::endl;
		if (muxer->Set_Track_Video(stream+1, width, height))
			std::cout << "-> Video error: failed setting track video" << std::endl;
	}

	uint8_t clamp(float value)
	{
		if (value < 0.f)
			return (uint8_t)0;
		else if (value > 255.f)
			return (uint8_t)255;
		else
			return (uint8_t)value;
	}

	void RGBA_to_YUV(const uint8_t *rgba_array, uint8_t *yuv_420_array, int luma_size, int width, int height)
	{
		for (int i = 0 ; i < luma_size ; i++) { // Luma Y'
			yuv_420_array[i] = clamp(static_cast<float>(0.299*rgba_array[4*i] + 0.587*rgba_array[4*i+1] + 0.114*rgba_array[4*i+2]));
		}
		int i = 0;
		for (int y = 0 ; y < height ; y+=2) {
			for (int x = 0 ; x < width ; x+=2) { // Chrominance U
				float sum = 0;
				for (int u = 0 ; u < 2 ; u++) {
					for (int v = 0 ; v < 2 ; v++) {
						int index = 4*(width*(y+v)+x+u);
						sum += static_cast<float>(-0.147*rgba_array[index] - 0.289*rgba_array[index+1] + 0.436*rgba_array[index+2]);
					}
				}
				yuv_420_array[luma_size+i] = clamp(128.f + sum / 4);
				i++;
			}
		}
		for (int y = 0 ; y < height ; y+=2) {
			for (int x = 0 ; x < width ; x+=2) { // Chrominance V
				float sum = 0;
				for (int u = 0 ; u < 2 ; u++) {
					for (int v = 0 ; v < 2 ; v++) {
						int index = 4*(width*(y+v)+x+u);
						sum += static_cast<float>(0.615*rgba_array[index] - 0.515*rgba_array[index+1] - 0.100*rgba_array[index+2]);
					}
				}
				yuv_420_array[luma_size+i] = clamp(128.f + sum / 4);
				i++;
			}
		}
	}

public:
	Video(mkvwriter::MatroskaMuxer *muxer, int stream, /*char **argv, */float V_fps):
		muxer(muxer),
		stream(stream),
		fps(V_fps)
	{
		codecId = "V_MPEG4/ISO/AVC";

		auto screen = sf::VideoMode::getDesktopMode();
		auto screen_size = sf::Vector2f(screen.width, screen.height);
		width = screen_size.x;
		height = screen_size.y;
		//sf::RenderWindow window(sf::VideoMode(screen), "Squares", sf::Style::Default);
		//if (argv[2] != std::string("large"))
		//	window.setSize(sf::Vector2u(1, 1));

		if (!r_texture.create(width, height))
			std::cout << "Video error: failed creating render texture" << std::endl;
		std::cout << "Render texture initialized" << std::endl;
		std::cout << "width: " << width << " | height: " << height << std::endl;

		rect_0.setSize(sf::Vector2f(200, 200));
		rect_0.setOutlineColor(sf::Color::Red);
		rect_0.setOutlineThickness(20);
		rect_0.setPosition(199, 200);
		rect_1.setSize(sf::Vector2f(200, 200));
		rect_1.setOutlineColor(sf::Color::Green);
		rect_1.setOutlineThickness(20);
		rect_1.setPosition(599, 200);
		rect_2.setSize(sf::Vector2f(200, 200));
		rect_2.setOutlineColor(sf::Color::Blue);
		rect_2.setOutlineThickness(20);
		rect_2.setPosition(199, 600);
		rect_3.setSize(sf::Vector2f(200, 200));
		rect_3.setOutlineColor(sf::Color::Yellow);
		rect_3.setOutlineThickness(20);
		rect_3.setPosition(599, 600);

		SetStreamHeader();
		if (!muxer->WriteHeaders())
			std::cout << "Video error: failed writing headers" << std::endl;
		std::cout << "Video headers written" << std::endl;

		x264_param_t param;

		//#ifdef _WIN32
		//	_setmode( _fileno( stdin ),  _O_BINARY );
		//	_setmode( _fileno( stdout ), _O_BINARY );
		//	_setmode( _fileno( stderr ), _O_BINARY );
		//#endif

		if (x264_param_default_preset( &param, "medium", NULL ) < 0)
			std::cout << "Video error: failed presetting x264 default param" << std::endl;

		param.i_bitdepth = 8;
		param.i_csp = X264_CSP_I420;
		param.i_width = width;
		param.i_height = height;
		param.b_vfr_input = 0;
		param.b_repeat_headers = 1;
		param.b_annexb = 1;

		if (x264_param_apply_profile(&param, "high") < 0)
			std::cout << "Video error: failed applying profile to x264 default param" << std::endl;

		if (x264_picture_alloc(&pic, param.i_csp, param.i_width, param.i_height) < 0)
			std::cout << "Video error: failed allocating x264 picture" << std::endl;

		h = x264_encoder_open(&param);
		if (!h)
			std::cout << "Video error: failed opening x264 encoder" << std::endl;

		luma_size = width * height;
		std::cout << "luma_size: " << luma_size << std::endl;
	}

	int encode_video_frame(uint64_t frame_count, uint64_t &written_frame_count)
	{
		rect_0.move(sf::Vector2f(25, 0));
		rect_1.move(sf::Vector2f(25, 0));
		rect_2.move(sf::Vector2f(25, 0));
		rect_3.move(sf::Vector2f(25, 0));

		r_texture.clear(sf::Color(128, 128, 255));
		r_texture.draw(rect_0);
		r_texture.draw(rect_1);
		r_texture.draw(rect_2);
		r_texture.draw(rect_3);
		r_texture.display();
		uint8_t *rgba_array = (uint8_t*)malloc(luma_size*4);
		memcpy(rgba_array, r_texture.getTexture().copyToImage().getPixelsPtr(), 4*luma_size);
		//const uint8_t *rgba_array = r_texture.getTexture().copyToImage().getPixelsPtr();
		uint8_t *yuv_420_array = (uint8_t*)malloc(luma_size*3/2);
		RGBA_to_YUV(rgba_array, yuv_420_array, luma_size, width, height);

		memcpy(pic.img.plane[0], yuv_420_array, luma_size);
		memcpy(pic.img.plane[1], yuv_420_array+luma_size, luma_size/4);
		memcpy(pic.img.plane[2], yuv_420_array+luma_size*5/4, luma_size/4);

		//window.clear(sf::Color::Black);
		//sf::Sprite sprite(r_texture.getTexture());
		//window.draw(sprite);
		//window.display();

		free(rgba_array);
		free(yuv_420_array);

		pic.i_pts = written_frame_count;
		int i_frame_size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
		std::cout << frame_count << ":" << i_frame_size << " ";
		if (i_frame_size < 0)
			return 0;
		else if (i_frame_size) {
			uint64 ts  = static_cast<uint64>(written_frame_count * 1000.0f / fps);
			uint64 dur = static_cast<uint64>(1000.0f / fps);
			int ref = 0; // or MatroskaMuxer::REFERENCE_PREV_FRAME
			muxer->AddFrame(stream+1, ts, dur, nal->p_payload, i_frame_size, ref);
			written_frame_count++;
		}

		return i_frame_size;
	}

	void close_window(void)
	{
		window.close();
	}

	int flush_frames(uint64_t &written_frame_count)
	{
		std::cout << "Number of frames written until flushing: " << written_frame_count << std::endl;

		while (x264_encoder_delayed_frames(h)) {
			int i_frame_size = x264_encoder_encode(h, &nal, &i_nal, NULL, &pic_out);
			std::cout << written_frame_count << ":" << i_frame_size << " ";
			if (i_frame_size < 0)
				return 0;
			else if (i_frame_size) {
				uint64 ts  = static_cast<uint64>(written_frame_count * 1000.0f / fps);
				uint64 dur = static_cast<uint64>(1000.0f / fps);
				int ref = 0; // or MatroskaMuxer::REFERENCE_PREV_FRAME
				muxer->AddFrame(stream+1, ts, dur, nal->p_payload, i_frame_size, ref);
				written_frame_count++;
			}
		}
		std::cout << std::endl;

		return written_frame_count;
	}

	void destroy_video_stream(void)
	{
		x264_encoder_close(h);
		x264_picture_clean(&pic);
	}
};