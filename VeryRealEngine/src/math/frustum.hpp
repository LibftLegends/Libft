/**
 * @file frustum.hpp
 * @brief A camera's view frustum as 6 world-space planes.
 *
 * Each stored as (a, b, c, d) with the "inside" half-space defined by
 * a*x+b*y+c*z+d >= 0. Extracted directly from a combined view-projection
 * matrix via the standard Gribb/Hartmann method — this works on any
 * composed clip matrix without needing the projection's individual
 * fov/aspect/near/far parameters, so it's agnostic to whether the caller
 * used perspective() or orthographic() to build it.
 */
#pragma once

#include "../vre.hpp"
#include "aabb.hpp"
#include "mat4.hpp"

namespace vre
{
class Frustum
{
  public:
	Frustum();
	Frustum(const Frustum &other);
	Frustum &operator=(const Frustum &other);
	~Frustum();

	/// @return The frustum described by clip matrix `view_projection`.
	static Frustum from_view_projection(const mat4 &view_projection);

	/**
		* @brief Conservative frustum/AABB intersection test.
		*
		* False only if the box is entirely on the outside of at least one
		* plane. May return true for a handful of boxes just outside the
		* frustum near a corner (the standard AABB-vs-frustum false-positive
		* case) — never a false negative, which is the direction that would
		* actually be visibly wrong (popping).
		* @param box World-space box to test.
		* @return true if `box` might be visible (is not conclusively
		* outside the frustum).
		*/
	bool intersects_aabb(const AABB &box) const;

  private:
	void set_plane(int index, const float row_a[4], const float row_b[4],
		float sign);
	/** @return Element (`row`, `col`) of column-major matrix `m`. */
	static float element(const mat4 &m, int row, int col);

	/** Left, Right, Bottom, Top, Near, Far, in that order. */
	float _planes[6][4];
};

} // namespace vre
