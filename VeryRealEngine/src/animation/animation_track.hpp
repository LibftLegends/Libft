/**
 * @file animation_track.hpp
 * @brief One bone's full set of keyframes over an AnimationClip's duration.
 * Keyframes must be sorted by time.
 */
#pragma once

#include "../vre.hpp"
#include "keyframe.hpp"

namespace vre
{

class AnimationTrack
{
  public:
	AnimationTrack();
	AnimationTrack(const AnimationTrack &other);
	AnimationTrack &operator=(const AnimationTrack &other);
	~AnimationTrack();

	uint32_t bone_index() const;
	void set_bone_index(uint32_t value);

	std::vector<Keyframe> &keyframes();
	const std::vector<Keyframe> &keyframes() const;

  private:
	uint32_t _bone_index;
	std::vector<Keyframe> _keyframes;
};

} // namespace vre
