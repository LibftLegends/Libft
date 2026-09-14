/**
 * @file skinned_asset.hpp

	* @brief Everything one skinned asset needs: geometry plus the skeleton/clip that animates it.
 */
#pragma once

#include "../animation/skeleton.hpp"
#include "../vre.hpp"
#include "mesh_data.hpp"

namespace vre
{

class SkinnedAsset
{
  public:
	SkinnedAsset();
	SkinnedAsset(const SkinnedAsset &other);
	SkinnedAsset &operator=(const SkinnedAsset &other);
	~SkinnedAsset();

	MeshData &mesh();
	const MeshData &mesh() const;

	Skeleton &skeleton();
	const Skeleton &skeleton() const;

	AnimationClip &clip();
	const AnimationClip &clip() const;

  private:
	MeshData _mesh;
	Skeleton _skeleton;
	AnimationClip _clip;
};

} // namespace vre
