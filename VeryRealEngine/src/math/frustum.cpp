#include "frustum.hpp"

namespace vre
{

Frustum::Frustum()
{
	std::memset(_planes, 0, sizeof(_planes));
}

Frustum::Frustum(const Frustum &other)
{
	std::memcpy(_planes, other._planes, sizeof(_planes));
}

Frustum &Frustum::operator=(const Frustum &other)
{
	if (this != &other)
		std::memcpy(_planes, other._planes, sizeof(_planes));
	return (*this);
}

Frustum::~Frustum()
{
}

void Frustum::set_plane(int index, const float row_a[4], const float row_b[4],
	float sign)
{
	float	a;
	float	b;
	float	c;
	float	d;
	float	length;

	a = row_a[0] + sign * row_b[0];
	b = row_a[1] + sign * row_b[1];
	c = row_a[2] + sign * row_b[2];
	d = row_a[3] + sign * row_b[3];
	length = std::sqrt(a * a + b * b + c * c);
	if (length > 1e-8f)
	{
		a /= length;
		b /= length;
		c /= length;
		d /= length;
	}
	_planes[index][0] = a;
	_planes[index][1] = b;
	_planes[index][2] = c;
	_planes[index][3] = d;
}

float Frustum::element(const mat4 &m, int row, int col)
{
	// Column-major storage (m(col*4+row)): component `col` of row `row` is m(row + col*4).
	return (m.m(row + col * 4));
}

Frustum Frustum::from_view_projection(const mat4 &view_projection)
{
	Frustum				frustum;
	float				row0[4] = {element(view_projection, 0, 0), element(view_projection, 0, 1),
							element(view_projection, 0, 2), element(view_projection, 0, 3)};
	float				row1[4] = {element(view_projection, 1, 0), element(view_projection, 1, 1),
							element(view_projection, 1, 2), element(view_projection, 1, 3)};
	float				row2[4] = {element(view_projection, 2, 0), element(view_projection, 2, 1),
							element(view_projection, 2, 2), element(view_projection, 2, 3)};
	float				row3[4] = {element(view_projection, 3, 0), element(view_projection, 3, 1),
							element(view_projection, 3, 2), element(view_projection, 3, 3)};
	static const float	zero_row[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	// Left/Right/Bottom/Top: standard row3 +/- row0/row1, unaffected by
	// Vulkan's [0,1] depth range (only the near plane below cares about that).
	frustum.set_plane(0, row3, row0, 1.0f);  // Left   = row3 + row0
	frustum.set_plane(1, row3, row0, -1.0f); // Right  = row3 - row0
	frustum.set_plane(2, row3, row1, 1.0f);  // Bottom = row3 + row1
	frustum.set_plane(3, row3, row1, -1.0f); // Top    = row3 - row1
	// Near = row2 directly (Vulkan clip-space z in [0,1]: the "z >= 0"
	// half-space, unlike OpenGL's [-1,1] range which needs row3+row2).
	frustum.set_plane(4, row2, zero_row, 1.0f);
	frustum.set_plane(5, row3, row2, -1.0f); // Far = row3 - row2
	return (frustum);
}

bool Frustum::intersects_aabb(const AABB &box) const
{
	vec3 center = (box.min() + box.max()) * 0.5f;
	vec3 extent = (box.max() - box.min()) * 0.5f;
	for (const auto &plane : _planes)
	{
		float distance = plane[0] * center.x() + plane[1] * center.y()
			+ plane[2] * center.z() + plane[3];
		float radius = extent.x() * std::fabs(plane[0]) + extent.y()
			* std::fabs(plane[1]) + extent.z() * std::fabs(plane[2]);
		if (distance + radius < 0.0f)
			return (false);
	}
	return (true);
}

} // namespace vre
