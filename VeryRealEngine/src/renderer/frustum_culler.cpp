#include "../math/aabb.hpp"
#include "../math/frustum.hpp"
#include "frustum_culler.hpp"

namespace vre
{

FrustumCuller::FrustumCuller()
{
}

FrustumCuller::FrustumCuller(const FrustumCuller & /*other*/)
{
}

FrustumCuller &FrustumCuller::operator=(const FrustumCuller & /*other*/)
{
	return (*this);
}

FrustumCuller::~FrustumCuller()
{
}

std::vector<RenderItem> FrustumCuller::cull(const mat4 &view,
	const mat4 &projection, const std::vector<RenderItem> &items,
	const MeshRegistry &mesh_registry) const
{
	Frustum camera_frustum = Frustum::from_view_projection(mat4::multiply(projection,
				view));
	std::vector<RenderItem> visible_items;
	visible_items.reserve(items.size());
	for (const auto &item : items)
	{
		AABB world_bounds = AABB::transform(mesh_registry.local_bounds(item.mesh()),
				item.model());
		if (camera_frustum.intersects_aabb(world_bounds))
			visible_items.push_back(item);
	}
	return (visible_items);
}

} // namespace vre
