#include "animation_track.hpp"

namespace vre
{

AnimationTrack::AnimationTrack() : _bone_index(0)
{
}

AnimationTrack::AnimationTrack(const AnimationTrack &other) : _bone_index(other._bone_index),
	_keyframes(other._keyframes)
{
}

AnimationTrack &AnimationTrack::operator=(const AnimationTrack &other)
{
	if (this != &other)
	{
		_bone_index = other._bone_index;
		_keyframes = other._keyframes;
	}
	return (*this);
}

AnimationTrack::~AnimationTrack()
{
}

uint32_t AnimationTrack::bone_index() const
{
	return (_bone_index);
}

void AnimationTrack::set_bone_index(uint32_t value)
{
	_bone_index = value;
}

std::vector<Keyframe> &AnimationTrack::keyframes()
{
	return (_keyframes);
}

const std::vector<Keyframe> &AnimationTrack::keyframes() const
{
	return (_keyframes);
}

} // namespace vre
