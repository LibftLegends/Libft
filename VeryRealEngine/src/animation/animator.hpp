/**
 * @file animator.hpp
 * @brief Plays one AnimationClip on one Skeleton: advances a clock and
 * produces the resulting skinning matrices each frame.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "animation_clip.hpp"
#include "skeleton.hpp"

namespace vre
{

class Animator
{
  public:
	Animator();
	Animator(const Animator &other);
	Animator &operator=(const Animator &other);
	~Animator();

	Animator(const Skeleton *skeleton, const AnimationClip *clip);

	/** Advances playback time by `delta_seconds`, looping at the clip's duration. */
	void update(float delta_seconds);

	/**
		* @brief Computes this frame's skinning matrix for every bone:
		* `animated_world(bone) * inverse(bind_world(bone))` — the standard
		* skinning-matrix formula, so a vertex bound at the bind pose ends
		* up exactly where the animated skeleton currently puts that bone,
		* not offset by the bind pose's own transform as well.
		* @param out_matrices Resized to `_skeleton->bones().size()` and filled in bone order.
		*/
	void compute_bone_matrices(std::vector<mat4> *out_matrices) const;

  private:
	/**
		* Linearly interpolates a track's rotation at `time`, clamping at
		* both ends rather than extrapolating past the first/last keyframe.
		*/
	static vec3 sample_track(const AnimationTrack &track, float time);

	const Skeleton *_skeleton;
	const AnimationClip *_clip;
	float _time;
};

} // namespace vre
