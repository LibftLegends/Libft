#include "shadowlightmatrix.hpp"

namespace vre
{
ShadowLightMatrix::ShadowLightMatrix()
{
}

ShadowLightMatrix::ShadowLightMatrix(const ShadowLightMatrix &)
{
}

ShadowLightMatrix &ShadowLightMatrix::operator=(const ShadowLightMatrix &)
{
	return (*this);
}

ShadowLightMatrix::~ShadowLightMatrix()
{
}

mat4 ShadowLightMatrix::compute(const Light &shadow_caster)
{
	vec3 scene_center(0.0f, 0.5f, 0.0f);
	vec3 up(0.0f, 1.0f, 0.0f);

	if (shadow_caster.type() == Light::Type::Point)
	{
		// Aiming straight down (the natural default for a ceiling light)
		// was tried and reverted: it views every vertical wall at a
		// near-maximum grazing angle from the shadow camera's perspective,
		// which — even with the normal-offset bias in mesh.vert — still
		// rendered walls as uniformly, incorrectly dark in the two-room
		// house scene. Aiming at a shared point diagonally instead keeps
		// most walls at a shallower, more forgiving angle, which is what
		// actually renders correctly here; this remains scene-shaped
		// (assumes something worth lighting near this coordinate), a
		// known simplification alongside the missing scene-bounds AABB.
		vec3 to_center = vec3::normalize(scene_center
				- shadow_caster.direction_or_position());
		if (std::fabs(vec3::dot(to_center, up)) > 0.99f)
			up = vec3(0.0f, 0.0f, 1.0f);

		mat4 light_view = mat4::look_at(shadow_caster.direction_or_position(),
				scene_center, up);
		mat4 light_projection = mat4::perspective(1.6f /* ~92 degrees */, 1.0f,
				0.1f, 20.0f);
		return (mat4::multiply(light_projection, light_view));
	}

	vec3 light_direction = vec3::normalize(
			shadow_caster.direction_or_position());
	if (std::fabs(vec3::dot(light_direction, up)) > 0.99f)
		up = vec3(0.0f, 0.0f, 1.0f);
			// avoid a degenerate look_at when the light is near-vertical

	vec3 light_position = scene_center - light_direction * 10.0f;
	mat4 light_view = mat4::look_at(light_position, scene_center, up);
	mat4 light_projection = mat4::orthographic(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f,
			20.0f);
	return (mat4::multiply(light_projection, light_view));
}

} // namespace vre
