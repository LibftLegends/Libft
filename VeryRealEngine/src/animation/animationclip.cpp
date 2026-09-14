#include "animationclip.hpp"

namespace vre
{
AnimationClip::AnimationClip() : _duration(1.0f)
{
}

AnimationClip::AnimationClip(const AnimationClip &other) :
	_duration(other._duration), _tracks(other._tracks)
{
}

AnimationClip &AnimationClip::operator=(const AnimationClip &other)
{
	if (this != &other)
	{
		_duration = other._duration;
		_tracks = other._tracks;
	}
	return (*this);
}

AnimationClip::~AnimationClip()
{
}

float AnimationClip::duration() const
{
	return (_duration);
}

void AnimationClip::set_duration(float value)
{
	_duration = value;
}

std::vector<AnimationTrack> &AnimationClip::tracks()
{
	return (_tracks);
}

const std::vector<AnimationTrack> &AnimationClip::tracks() const
{
	return (_tracks);
}

} // namespace vre
