#include <cmath>
//#include <vector>
//#include <fstream>
#include <iostream>
//#include <iterator>

#ifdef _WIN32
#include <io.h>       /* _setmode() */
#include <fcntl.h>    /* _O_BINARY */
#endif

#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp> //
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp> //
#include <SFML/Graphics/Sprite.hpp> //

#include "mkvwriter/MatroskaMuxer.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
#include <x264.h>

static inline constexpr size_t A_READSIZE = 4048;
static inline constexpr double pi = 3.141592653589793;

class Audio
{
	mkvwriter::MatroskaMuxer *muxer;

	int stream;
	uint32_t sample_rate;
	uint32_t total_samples;
	uint32_t left_samples;
	uint32_t written_samples;
	bool codecPrivateInitialized = false;
	binary *codecPrivate = nullptr;
	uint32 codecPrivateSize = 0;
	FLAC__StreamEncoder *encoder;
	uint64_t total_samples_in_frame;
	FLAC__int32 *buffer = nullptr;

	void SetStreamHeader(mkvwriter::MatroskaMuxer *muxer, int stream, std::string codecId, uint8_t num_channels, double sample_rate, uint8_t sample_size,
	const binary *codecPrivate, uint32 codecPrivateSize)
	{
		std::cout << "Writing " << codecId << " to track " << stream+1 << std::endl;

		if (muxer->Set_Track_CodecID(stream+1, codecId))
			std::cout << "-> Audio error: failed setting track codec ID" << std::endl;
		if (muxer->Set_Track_CodecPrivate(stream+1, codecPrivate, codecPrivateSize))
			std::cout << "-> Audio error: failed setting track codec private" << std::endl;
		if (muxer->Set_Track_Audio(stream+1, num_channels, sample_rate, sample_size, sample_rate))
			std::cout << "-> Audio error: failed setting track audio" << std::endl;
	}

	FLAC__StreamEncoderWriteStatus write(const FLAC__byte *buffer, size_t bytes)
	{
		if (!codecPrivateInitialized) {
			auto was_size = codecPrivateSize;
			codecPrivateSize += bytes;
			codecPrivate = (binary*)realloc(codecPrivate, codecPrivateSize);
			memcpy(reinterpret_cast<char*>(codecPrivate) + was_size, buffer, bytes);
			std::cout << codecPrivateSize << " ";
		}
		else {
			uint64_t time_code = 1000 * written_samples / sample_rate;
			uint32_t duration = 1000 * A_READSIZE / sample_rate;
			int ref = 0; // or MatroskaMuxer::REFERENCE_PREV_FRAME
			
			if (muxer->AddFrame(stream+1, time_code, duration, buffer, bytes, ref))
				std::cout << "-> Audio error: failed adding frame" << std::endl;
			written_samples += A_READSIZE;
			//std::cout << "-> Written samples updated: " << written_samples << " | " << bytes << std::endl << std::endl;
		}

		return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
	}

public:
	Audio(mkvwriter::MatroskaMuxer *muxer, float V_fps):
		muxer(muxer)
	{
		stream = 0;
		std::string codecId = "A_FLAC";

		uint32_t channels = 1;
		uint32_t bps = 16;
		sample_rate = 44100;
		total_samples = 441000;
		left_samples = total_samples;
		written_samples = 0;
		//codecPrivateInitialized = false;

		encoder = FLAC__stream_encoder_new();
		if (encoder == NULL)
			std::cout << "-> Audio error: failed creating encoder" << std::endl;
		FLAC__stream_encoder_set_verify(encoder, true);
		FLAC__stream_encoder_set_channels(encoder, channels);
		FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
		FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
		FLAC__stream_encoder_set_compression_level(encoder, 5);
		FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);
		if (0)
			std::cout << "-> Audio error: failed setting stream encoder" << std::endl;
		std::cout << "Audio stream encoder set" << std::endl;

		FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(encoder, write_ll, NULL, NULL, NULL, this);
		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
			std::cout << "-> Audio error: failed initializing encoder" << std::endl;
		codecPrivateInitialized = true;
		std::cout << "Audio stream encoder initialized" << std::endl;

		SetStreamHeader(muxer, stream, codecId, channels, sample_rate, bps, codecPrivate, codecPrivateSize);
		if (!muxer->WriteHeaders())
			std::cout << "Audio error: failed writing headers" << std::endl;
		free(codecPrivate);
		std::cout << "Audio headers written" << std::endl << std::endl;

		total_samples_in_frame = sample_rate / V_fps;
		buffer = new FLAC__int32[A_READSIZE];
	}

	~Audio(void)
	{
		delete[] buffer;
		FLAC__stream_encoder_finish(encoder);
		FLAC__stream_encoder_delete(encoder);
	}

	static FLAC__StreamEncoderWriteStatus write_ll(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
	unsigned current_frame, void *client_data)
	{		
		auto a = encoder;
		if (a) current_frame = samples;
		return reinterpret_cast<Audio*>(client_data)->write(buffer, bytes);
	}

	int encode_audio_frame(long double &x, long double &x_freq, double &freq, double w_freq)
	{
		uint64_t left_samples_in_frame = total_samples_in_frame;
		while (left_samples_in_frame > 0) {
			uint32_t samples = (A_READSIZE < left_samples_in_frame ? A_READSIZE : left_samples_in_frame);
			for (uint32_t k = 0 ; k < samples ; k++) {
				buffer[k] = static_cast<FLAC__int32>(std::sin(x) * 3270);
				x += 2 * pi * freq / sample_rate;
				freq = 220*std::sin(x_freq) + 440;
				x_freq += 2 * pi * w_freq / sample_rate;
			}
			left_samples_in_frame -= samples;
		}
		if (!FLAC__stream_encoder_process_interleaved(encoder, buffer, total_samples_in_frame))
			std::cout << "Error while encoding audio" << std::endl;

		return total_samples_in_frame;
	}
};

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

	int stream;
	x264_picture_t pic;
	x264_picture_t pic_out;
	x264_t *h;
	x264_nal_t *nal;
	int i_nal;

	float fps;

	void SetStreamHeader(mkvwriter::MatroskaMuxer *muxer, int stream, int width, int height, std::string codecId)
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
		for (int i = 0 ; i < luma_size ; i++) { /* Luma Y' */
			yuv_420_array[i] = clamp(roundf(0.299*rgba_array[4*i] + 0.587*rgba_array[4*i+1] + 0.114*rgba_array[4*i+2]));
		}
		int i = 0;
		for (int y = 0 ; y < height ; y+=2) {
			for (int x = 0 ; x < width ; x+=2) { /* Chrominance U */
				float sum = 0;
				for (int u = 0 ; u < 2 ; u++) {
					for (int v = 0 ; v < 2 ; v++) {
						int index = 4*(width*(y+v)+x+u);
						sum += roundf(-0.147*rgba_array[index] - 0.289*rgba_array[index+1] + 0.436*rgba_array[index+2]);
					}
				}
				yuv_420_array[luma_size+i] = clamp(128.f + sum / 4);
				i++;
			}
		}
		for (int y = 0 ; y < height ; y+=2) {
			for (int x = 0 ; x < width ; x+=2) { /* Chrominance V */
				float sum = 0;
				for (int u = 0 ; u < 2 ; u++) {
					for (int v = 0 ; v < 2 ; v++) {
						int index = 4*(width*(y+v)+x+u);
						sum += roundf(0.615*rgba_array[index] - 0.515*rgba_array[index+1] - 0.100*rgba_array[index+2]);
					}
				}
				yuv_420_array[luma_size+i] = clamp(128.f + sum / 4);
				i++;
			}
		}
	}

public:
	Video(mkvwriter::MatroskaMuxer *muxer, char **argv, float V_fps):
		muxer(muxer)
	{
		stream = 1;
		std::string codecId = "V_MPEG4/ISO/AVC";
		fps = V_fps;

		auto screen = sf::VideoMode::getDesktopMode();
		auto screen_size = sf::Vector2f(screen.width, screen.height);
		width = screen_size.x;
		height = screen_size.y;
		sf::RenderWindow window(sf::VideoMode(screen), "Squares", sf::Style::Default);
		if (argv[2] != std::string("large"))
			window.setSize(sf::Vector2u(1, 1));

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

		SetStreamHeader(muxer, stream, width, height, codecId);
		if (!muxer->WriteHeaders())
			std::cout << "Video error: failed writing headers" << std::endl;
		std::cout << "Video headers written" << std::endl;
		
		x264_param_t param;

		#ifdef _WIN32
			_setmode( _fileno( stdin ),  _O_BINARY );
			_setmode( _fileno( stdout ), _O_BINARY );
			_setmode( _fileno( stderr ), _O_BINARY );
		#endif

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

	~Video(void)
	{
		x264_encoder_close(h);
		x264_picture_clean(&pic);
	}

	int encode_video_frame(uint64_t frame_count, uint64_t &written_frame_count)
	{
		rect_0.move(sf::Vector2f(25, 0));
		rect_1.move(sf::Vector2f(25, 0));
		rect_2.move(sf::Vector2f(25, 0));
		rect_3.move(sf::Vector2f(25, 0));

		r_texture.getSize();
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

		window.clear(sf::Color::Black);
		sf::Sprite sprite(r_texture.getTexture());
		window.draw(sprite);
		window.display();

		free(rgba_array);
		free(yuv_420_array);

		pic.i_pts = written_frame_count;
		int i_frame_size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
		std::cout << frame_count << ":" << i_frame_size << " ";
		if( i_frame_size < 0 )
			return 0;
		else if( i_frame_size )
		{
			uint64 ts  = roundf(written_frame_count * 1000.0f / fps);
			uint64 dur = roundf(1000.0f / fps);
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

	int flush_frames(uint64_t frame_count, uint64_t &written_frame_count)
	{
		std::cout << "Number of frames written until flushing: " << written_frame_count << std::endl;

		while (x264_encoder_delayed_frames(h)) {
			int i_frame_size = x264_encoder_encode( h, &nal, &i_nal, NULL, &pic_out );
			std::cout << frame_count << ":" << i_frame_size << " ";
			if( i_frame_size < 0 )
				return 0;
			else if( i_frame_size )
			{
				uint64 ts  = roundf(frame_count * 1000.0f / fps);
				uint64 dur = roundf(1000.0f / fps);
				int ref = 0; // or MatroskaMuxer::REFERENCE_PREV_FRAME
				muxer->AddFrame(stream+1, ts, dur, nal->p_payload, i_frame_size, ref);
				written_frame_count++;
			}
		}

		return written_frame_count;
	}
};

class Studios
{
public:
	Studios(int argc, char **argv)
	{	
		mkvwriter::MatroskaMuxer muxer;
		if (argc < 3)
			std::cerr << "Error: Insufficient arguments provided\n" <<
				"Usage: muxer <output_filename: file.mkv> <window mode: large || small>\n";
		std::string outfile = argv[1];
		if (muxer.Set_OutputFilename(outfile))
			std::cout << "-> Audio error: failed setting output filename" << std::endl;

		float V_fps = 20;

					/* ===============================> A U D I O <=============================== */
		auto A = Audio(&muxer, V_fps);
		
					/* ===============================> V I D E O <=============================== */
		auto V = Video(&muxer, argv, V_fps);
		
					/* ==============================> S T U D I O <=============================== */
		long double A_x = 0;
		long double A_x_freq = 0;
		double A_freq = 440;
		double A_w_freq = 0.5;

		uint64_t total_frame_count = 60;
		uint64_t frame_count = 0;
		uint64_t written_frame_count = 0;

		std::cout << "loop begins" << std::endl;
		while (frame_count < total_frame_count) {
			A.encode_audio_frame(A_x, A_x_freq, A_freq, A_w_freq);
			V.encode_video_frame(frame_count, written_frame_count);
			frame_count++;
		}
		V.close_window();
		V.flush_frames(frame_count, written_frame_count);

		//A.~Audio();
		//V.~Video();

		muxer.CloseFile();
	}
};

int main( int argc, char *argv[] )
{
	auto studios = Studios(argc, argv);

	//./STD.exe file.mkv

	return 0;
}