/**
 * @file skinnedmeshanimationreader.hpp
 * @brief Reads a skinned-mesh JSON file's "animation" object into an
 * AnimationClip.
 */
#pragma once

#include "../animation/animationclip.hpp"
#include "../vre.hpp"
#include "jsonvalue.hpp"

namespace vre
{
class SkinnedMeshAnimationReader
{
  public:
	SkinnedMeshAnimationReader();
	SkinnedMeshAnimationReader(const SkinnedMeshAnimationReader &other);
	SkinnedMeshAnimationReader &operator=(
		const SkinnedMeshAnimationReader &other);
	~SkinnedMeshAnimationReader();

	/**
		* @brief Reads `root`'s "animation" object (a "duration" and a
		* "tracks" array, each with a "bone" index and a "keyframes" array)
		* into `out_clip`.
		* @return false if "animation" is absent or not an object
		* (`out_clip` left untouched).
		*/
	static bool read(const JsonValue &root, AnimationClip *out_clip);
};

} // namespace vre
