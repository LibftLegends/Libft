/**
 * @file skeleton.hpp
 * @brief A bone hierarchy plus its precomputed bind pose, shared by every
 * Animator that plays a clip on this skeleton.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "bone.hpp"

namespace vre
{

class Skeleton
{
  public:
	Skeleton();
	Skeleton(const Skeleton &other);
	Skeleton &operator=(const Skeleton &other);
	~Skeleton();

	std::vector<Bone> &bones();
	const std::vector<Bone> &bones() const;

	/**
		* @brief Computes and caches each bone's inverse bind-pose world
		* matrix — call once after bones() is fully populated (see
		* SkinnedMeshLoader), before any Animator uses this skeleton.
		*/
	void compute_bind_pose();

	/** @return Bone `index`'s cached inverse bind-pose world matrix (see compute_bind_pose()). */
	const mat4 &bind_pose_inverse(size_t index) const;

  private:
	std::vector<Bone> _bones;
	std::vector<mat4> _bind_pose_inverse;
};

} // namespace vre
