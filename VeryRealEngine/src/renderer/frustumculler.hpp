/**
 * @file frustumculler.hpp
 * @brief Drops render items whose world-space bounding box doesn't
 * intersect the camera frustum, before they reach the GPU at all.
 */
#pragma once

#include "../math/aabb.hpp"
#include "../math/frustum.hpp"
#include "../math/mat4.hpp"
#include "../vre.hpp"
#include "../rendererresources/meshregistry.hpp"
#include "renderitem.hpp"

namespace vre
{
class FrustumCuller
{
  public:
	FrustumCuller();
	FrustumCuller(const FrustumCuller &other);
	FrustumCuller &operator=(const FrustumCuller &other);
	~FrustumCuller();

	/// @return The subset of `items` whose world-space bounds intersect
	/// the view*projection frustum.
	std::vector<RenderItem> cull(const mat4 &view, const mat4 &projection,
		const std::vector<RenderItem> &items,
		const MeshRegistry &mesh_registry) const;
};

} // namespace vre
