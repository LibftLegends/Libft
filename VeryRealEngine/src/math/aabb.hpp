/**
 * @file aabb.hpp
 * @brief Axis-aligned bounding box, used for both frustum and occlusion
 * culling (renderer.cpp).
 *
 * One is computed per mesh at load time in object-local space
 * (Renderer::load_mesh_from_obj), then re-derived in world space per draw
 * call via transform() below, using each RenderItem's model matrix.
 */
#pragma once

#include "../vre.hpp"
#include "mat4.hpp"
#include "vec3.hpp"

namespace vre
{
class AABB
{
  public:
	AABB();
	AABB(const AABB &other);
	AABB &operator=(const AABB &other);
	~AABB();

	AABB(const vec3 &min, const vec3 &max);

	const vec3 &min() const;
	const vec3 &max() const;

	/// @brief Grows this box, if needed, so it also encloses `point`.
	void encapsulate(const vec3 &point);

	/**
		* @brief Re-derives a tight world-space AABB from a local-space one
		* and an arbitrary (possibly rotated) affine transform, by
		* transforming all 8 corners and taking their bounds — simpler and
		* easier to verify correct than the axis-projection shortcut (Arvo's
		* method), and cheap enough at this engine's object counts (tens, not
		* millions).
		* @param local Object-local bounding box.
		* @param model World-space model matrix to apply.
		* @return The tight world-space AABB enclosing the transformed box.
		*/
	static AABB transform(const AABB &local, const mat4 &model);

  private:
	vec3 _min;
	vec3 _max;
};

} // namespace vre
