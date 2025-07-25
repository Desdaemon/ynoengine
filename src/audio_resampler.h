/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EP_AUDIO_RESAMPLER_H
#define EP_AUDIO_RESAMPLER_H

// Headers
// Don't remove the system.h include, prevents heap corruption for automake (preprocessor defines)
#include "audio_decoder.h"
#include "audio_decoder_base.h"
#include "system.h"
#include <string>
#include <memory>

#if defined(HAVE_LIBSPEEXDSP)
#include <speex/speex_resampler.h>
#elif defined(HAVE_LIBSAMPLERATE)
#include <samplerate.h>
#endif

/**
 * Audio resampler powered by Libspeexdsp or Libsamplerate
 * Wraps another decoder and provides resampling.
 */
class AudioResampler : public AudioDecoderBase {
public:
	/** Resampling quality */
	enum class Quality {
		High,
		Medium,
		Low
	};

	/**
	 * Constructs a resampler
	 *
	 * @param[in] decoder The decoder which provides samples to the resampler - will be owned by the resampler
	 * @param[in] quality Sets the quality rting of the resampler - higher quality implies slower filtering
	 */
	AudioResampler(std::unique_ptr<AudioDecoderBase> decoder, Quality quality = Quality::Low);

	/**
	 * Destroys the resampler as well as its owned ressources
	 */
	~AudioResampler();

	/**
	 * Wraps the status querying of the contained decoder.
	 * Used to make sure the underlying library is properly initialized.
	 *
	 * @return true if initializing was succesful, false otherwise
	 */
	bool WasInited() const override;

	/**
	 * Wraps the opening function of the contained decoder
	 *
	 * @param[in] file Filepointer to a file readable by the wrapped decoder
	 *
	 * @return Whether the operation was successful or not
	 */
	bool Open(Filesystem_Stream::InputStream stream) override;

	/**
	 * Forwards a pause to the wrapped decoder.
	 */
	void Pause() override;

	/**
	 * Forwards a resume to the wrapped decoder.
	 */
	void Resume() override;

	/**
	 * Obtains the volume of the wrapped decoder.
	 *
	 * @return pair of current left-right volume (from 0 - 100)
	 */
	StereoVolume GetVolume() const override;

	/**
	 * Sets the current volume of the wrapped decoder.
	 *
	 * @param volume (from 0-100)
	 */
	void SetVolume(int volume) override;

	/**
	 * Prepares a volume fade in/out effect.
	 * To do a fade out begin must be larger then end.
	 * Call Update to do the fade.
	 *
	 * @param end End volume (from 0-100)
	 * @param duration Fade duration in ms
	 */
	void SetFade(int end, std::chrono::milliseconds duration) override;

	/**
	 * Wraps the seek function of the contained decoder
	 * @note If the seek function of the wrapped decoder is
	 *  somewhat correlated to time the offset is not influenced by the resampling ratio
	 *
	 * @param offset Offset to seek to
	 * @param origin Position to seek from
	 *
	 * @return Whether seek was successful
	 */
	bool Seek(std::streamoff offset, std::ios_base::seekdir origin) override;

	/**
	 * Gets if the audio stream will loop when the stream finishes.
	 *
	 * @return if looping
	 */
	bool GetLooping() const override;

	/**
	 * Enables/Disables audio stream looping.
	 * When looping is enabled IsFinished will never return true and the stream
	 * auto-rewinds (assuming Rewind is supported)
	 *
	 * @param enable Enable/Disable looping
	 */
	void SetLooping(bool enable) override;

	/**
	 * Gets the number of loops
	 *
	 * @return loop count
	 */
	int GetLoopCount() const override;

	/**
	 * Gets the current pan/balance of the underlying decoder.
	 * 0 = full left, 50 = center, 100 = full right
	 */
	int GetBalance() const override;

	/**
	 * Sets the pan/balance of the underlying decoder.
	 */
	void SetBalance(int new_balance) override;

	/**
	 * Wraps the tell function of the contained decoder
	 *
	 * @return Position in the stream
	 */
	std::streampos Tell() const override;

	/**
	 * Wraps the GetTicks Function of the contained decoder
	 *
	 * @return Amount of MIDI ticks.
	 */
	int GetTicks() const override;

	/**
	 * Returns whether the resampled is exhausted and the audio stream is finished.
	 *
	 * @return true if the stream has reached it's end
	 */
	bool IsFinished() const override;

	/**
	 * Updates timing related audio code of the wrapped decoder.
	 *
	 * @param delta Time in us since the last call of this function.
	 */
	void Update(std::chrono::microseconds delta) override;

	/**
	 * Retrieves the format of the audio decoder.
	 *
	 * @param frequency Filled with the audio frequency
	 * @param format Filled with the audio format
	 * @param channels Filled with the amount of channels
	 */
	void GetFormat(int& frequency, AudioDecoder::Format& format, int& channels) const override;

	/**
	 * Requests a certain frame format from the resampler.
	 * Supported formats are:
	 *  * float,int16_t for libspeexdsp
	 *  * float for libsamplerate
	 * The channel setting is redirected to the wrapped decoder.
	 * The frequency setting controls the resampler.
	 *
	 * @param frequency Sample rate the resampler should output
	 * @param format Audio format the resampler should output
	 * @param channels Number of channels
	 * @return true when all settings were set, otherwise false (use GetFormat)
	 */
	bool SetFormat(int frequency, AudioDecoder::Format format, int channels) override;

	/**
	 * Gets the pitch multiplier.
	 *
	 * @return pitch multiplier
	 */
	int GetPitch() const override;

	/**
	 * Sets the pitch multiplier.
	 * 100 = normal speed
	 * 200 = double speed and so on
	 * If the pitch is handled by the resampler this setting controls the resampling in conjunction with the frequency.
	 *
	 * @param pitch Pitch multiplier to use
	 * @return true if pitch was set, false otherwise
	 */
	bool SetPitch(int pitch) override;

private:
	/**
	 * Called by the Decode functions to fill the buffer.
	 *
	 * @param buffer Buffer to fill
	 * @param length Buffer size
	 * @return number of bytes read or -1 on error
	 */
	int FillBuffer(uint8_t* buffer, int length) override;

	/**
	 * Internally used by the FillBuffer function if the output rate equals the input rate
	 */
	int FillBufferSameRate(uint8_t* buffer, int length);

	/**
	 * Internally used by the FillBuffer function if resampling is necessary
	 */
	int FillBufferDifferentRate(uint8_t* buffer, int length);

	std::unique_ptr<AudioDecoderBase> wrapped_decoder;
	bool pitch_handled_by_decoder = false;
	int pitch = 100;
	int sampling_quality;
	int lasterror;
	bool finished;

	int nr_of_channels;
	Format input_format;
	int input_rate;
	Format output_format;
	int output_rate;

	#if defined(HAVE_LIBSPEEXDSP)
		struct {
			spx_uint32_t input_frames, output_frames;
			spx_uint32_t input_frames_used, output_frames_gen;
			spx_uint32_t ratio_num, ratio_denom;
		} conversion_data;
		SpeexResamplerState * conversion_state = nullptr;
	#elif defined(HAVE_LIBSAMPLERATE)
		SRC_DATA conversion_data;
		SRC_STATE * conversion_state = nullptr;
	#endif

	/**
	 * A buffer needed for operations which can't be performed in place (e.g resampling)
	 * The size of the buffer defines the number of calls to the resampling algorithmn
	 * (In the cpp file sizeof is used therefore it can be adjusted to fit the available memory)
	 */
	uint8_t internal_buffer[256*sizeof(float)];

	bool mono_to_stereo_resample = false;
};

#endif
