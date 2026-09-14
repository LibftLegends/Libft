/**
 * @file animation_clip.hpp
 * @brief A named, timed collection of per-bone keyframe tracks — one
 * looping animation.
 */
#pragma once

#include "../vre.hpp"
#include "animation_track.hpp"

namespace vre
{

class AnimationClip
{
  public:
	AnimationClip();
	AnimationClip(const AnimationClip &other);
	AnimationClip &operator=(const AnimationClip &other);
	~AnimationClip();

	/** Seconds; playback loops back to 0 after this. */
	float duration() const;
	void set_duration(float value);

	/** Bones with no track here just hold their bind pose. */
	std::vector<AnimationTrack> &tracks();
	const std::vector<AnimationTrack> &tracks() const;

  private:
	float _duration;
	std::vector<AnimationTrack> _tracks;
};

} // namespace vre
