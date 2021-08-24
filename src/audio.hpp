#pragma once

#include <cmath>
#include <iostream>

#include "mkvwriter/MatroskaMuxer.h"
#include "FLAC/stream_encoder.h"

static inline constexpr size_t A_READSIZE = 4048;
static inline constexpr double pi = 3.141592653589793;

class Audio
{
	mkvwriter::MatroskaMuxer *muxer;

	std::string codecId;
	int stream;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t bps;
	uint32_t written_samples;
	bool codecPrivateInitialized = false;
	binary *codecPrivate = nullptr;
	uint32 codecPrivateSize = 0;
	FLAC__StreamEncoder *encoder;
	uint64_t total_samples_in_frame;
	FLAC__int32 *buffer = nullptr;

	void SetStreamHeader(void)
	{
		std::cout << "Writing " << codecId << " to track " << stream+1 << std::endl;

		if (muxer->Set_Track_CodecID(stream+1, codecId))
			std::cout << "-> Audio error: failed setting track codec ID" << std::endl;
		if (muxer->Set_Track_CodecPrivate(stream+1, codecPrivate, codecPrivateSize / 2))
			std::cout << "-> Audio error: failed setting track codec private" << std::endl;
		if (muxer->Set_Track_Audio(stream+1, channels, sample_rate, static_cast<uint16>(bps), sample_rate))
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

	static FLAC__StreamEncoderWriteStatus write_ll(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
	unsigned current_frame, void *client_data)
	{
		auto a = encoder;
		if (a) current_frame = samples;
		return reinterpret_cast<Audio*>(client_data)->write(buffer, bytes);
	}

public:
	Audio(mkvwriter::MatroskaMuxer *muxer, int stream, float V_fps):
		muxer(muxer),
		stream(stream)
	{
		codecId = "A_FLAC";
		channels = 1;
		bps = 16;
		sample_rate = 44100;
		written_samples = 0;

		encoder = FLAC__stream_encoder_new();
		if (encoder == NULL)
			std::cout << "-> Audio error: failed creating encoder" << std::endl;
		FLAC__stream_encoder_set_verify(encoder, true);
		FLAC__stream_encoder_set_channels(encoder, channels);
		FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
		FLAC__stream_encoder_set_sample_rate(encoder, sample_rate);
		FLAC__stream_encoder_set_compression_level(encoder, 5);
		FLAC__stream_encoder_set_total_samples_estimate(encoder, 0);
		if (0)
			std::cout << "-> Audio error: failed setting stream encoder" << std::endl;
		std::cout << "Audio stream encoder set" << std::endl;

		FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(encoder, write_ll, NULL, NULL, NULL, this);
		if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
			std::cout << "-> Audio error: failed initializing encoder" << std::endl;
		codecPrivateInitialized = true;
		std::cout << "Audio stream encoder initialized" << std::endl;

		SetStreamHeader();
		if (!muxer->WriteHeaders())
			std::cout << "Audio error: failed writing headers" << std::endl;
		free(codecPrivate);
		std::cout << "Audio headers written" << std::endl << std::endl;

		total_samples_in_frame = sample_rate / V_fps;
		buffer = new FLAC__int32[A_READSIZE];
	}

	void destroy_audio_stream(void)
	{
		delete[] buffer;
		FLAC__stream_encoder_finish(encoder);
		FLAC__stream_encoder_delete(encoder);
	}

	int encode_audio_frame(long double &x, long double &x_freq, double &freq, double w_freq)
	{
		uint64_t left_samples_in_frame = total_samples_in_frame;
		while (left_samples_in_frame > 0) {
			uint32_t samples = (A_READSIZE < left_samples_in_frame ? A_READSIZE : left_samples_in_frame);
			for (uint32_t k = 0 ; k < samples ; k++) {
				buffer[k] = static_cast<FLAC__int32>(std::sin(x) * 32700);
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