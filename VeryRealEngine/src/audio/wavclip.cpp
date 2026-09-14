#include "wavclip.hpp"

namespace vre
{
WavClip::WavClip() : _sample_rate(0), _channels(0)
{
}

WavClip::WavClip(const WavClip &other) : _samples(other._samples),
	_sample_rate(other._sample_rate), _channels(other._channels)
{
}

WavClip &WavClip::operator=(const WavClip &other)
{
	if (this != &other)
	{
		_samples = other._samples;
		_sample_rate = other._sample_rate;
		_channels = other._channels;
	}
	return (*this);
}

WavClip::~WavClip()
{
}

std::vector<int16_t> &WavClip::samples()
{
	return (_samples);
}

const std::vector<int16_t> &WavClip::samples() const
{
	return (_samples);
}

uint32_t WavClip::sample_rate() const
{
	return (_sample_rate);
}

void WavClip::set_sample_rate(uint32_t value)
{
	_sample_rate = value;
}

uint16_t WavClip::channels() const
{
	return (_channels);
}

void WavClip::set_channels(uint16_t value)
{
	_channels = value;
}

size_t WavClip::frame_count() const
{
	return (_channels > 0 ? _samples.size() / _channels : 0);
}

} // namespace vre
