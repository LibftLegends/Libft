/**
 * @file wavclip.hpp
 * @brief Decoded PCM audio: 16-bit signed samples, interleaved by channel
 * (frame 0 = samples[0..channels-1], frame 1 = samples[channels..2*channels-1],
	...).
 *
 * 8-bit source files are up-converted to 16-bit on load, so every consumer
 * (the mixer in particular) only ever deals with one sample format.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class WavClip
{
  public:
	WavClip();
	WavClip(const WavClip &other);
	WavClip &operator=(const WavClip &other);
	~WavClip();

	/** Interleaved 16-bit PCM samples. */
	std::vector<int16_t> &samples();
	const std::vector<int16_t> &samples() const;

	/** Samples per second per channel (e.g. 44100). */
	uint32_t sample_rate() const;
	void set_sample_rate(uint32_t value);

	/** 1 = mono, 2 = stereo. */
	uint16_t channels() const;
	void set_channels(uint16_t value);

	/** @return Total frames (one frame = one sample per channel),
	 * independent of channel count. */
	size_t frame_count() const;

  private:
	std::vector<int16_t> _samples;
	uint32_t _sample_rate;
	uint16_t _channels;
};

} // namespace vre
