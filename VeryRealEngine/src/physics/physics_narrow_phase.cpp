/**
 * @file physics_narrow_phase.cpp
 * @brief PhysicsWorld's exact box/sphere shape tests, split out of
 * physics_collision.cpp purely to keep each file under this project's
 * 250-line cap — both files define static methods of the same
 * PhysicsWorld class declared in physics_world.hpp.
 */
#include "physics_world.hpp"

namespace vre
{

PhysicsWorld::Contact PhysicsWorld::collide_box_box(const vec3 &pos_a,
	const vec3 &half_a, const vec3 &pos_b, const vec3 &half_b)
{
	Contact	contact;
	vec3	delta;
	float	overlap_x;
	float	overlap_y;
	float	overlap_z;

	delta = pos_b - pos_a;
	overlap_x = (half_a.x() + half_b.x()) - std::fabs(delta.x());
	if (overlap_x <= 0.0f)
		return (contact);
	overlap_y = (half_a.y() + half_b.y()) - std::fabs(delta.y());
	if (overlap_y <= 0.0f)
		return (contact);
	overlap_z = (half_a.z() + half_b.z()) - std::fabs(delta.z());
	if (overlap_z <= 0.0f)
		return (contact);
	// Minimum-translation axis: the axis with the smallest overlap is the
	// one separating the boxes with the least motion — standard AABB SAT.
	contact.valid = true;
	if (overlap_x <= overlap_y && overlap_x <= overlap_z)
	{
		contact.penetration = overlap_x;
		contact.normal = vec3(delta.x() < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
	}
	else if (overlap_y <= overlap_z)
	{
		contact.penetration = overlap_y;
		contact.normal = vec3(0.0f, delta.y() < 0.0f ? -1.0f : 1.0f, 0.0f);
	}
	else
	{
		contact.penetration = overlap_z;
		contact.normal = vec3(0.0f, 0.0f, delta.z() < 0.0f ? -1.0f : 1.0f);
	}
	return (contact);
}

PhysicsWorld::Contact PhysicsWorld::collide_sphere_sphere(const vec3 &pos_a,
	float radius_a, const vec3 &pos_b, float radius_b)
{
	Contact	contact;
	vec3	delta;
	float	distance;
	float	penetration;

	delta = pos_b - pos_a;
	distance = std::sqrt(vec3::dot(delta, delta));
	penetration = (radius_a + radius_b) - distance;
	if (penetration <= 0.0f)
		return (contact);
	contact.valid = true;
	contact.penetration = penetration;
	contact.normal = (distance > 1e-6f) ? (delta * (1.0f
				/ distance)) : vec3(0.0f, 1.0f, 0.0f);
	return (contact);
}

PhysicsWorld::Contact PhysicsWorld::collide_box_sphere(const vec3 &box_pos,
	const vec3 &half_extents, const vec3 &sphere_pos, float radius)
{
	Contact	contact;
	vec3	local;
	vec3	delta;
	float	distance;
	float	penetration;
	float	px;
	float	py;
	float	pz;

	local = sphere_pos - box_pos;
	vec3 closest(std::clamp(local.x(), -half_extents.x(), half_extents.x()),
		std::clamp(local.y(), -half_extents.y(), half_extents.y()),
		std::clamp(local.z(), -half_extents.z(), half_extents.z()));
	delta = local - closest;
	distance = std::sqrt(vec3::dot(delta, delta));
	if (distance > 1e-6f)
	{
		penetration = radius - distance;
		if (penetration <= 0.0f)
			return (contact);
		contact.valid = true;
		contact.penetration = penetration;
		contact.normal = delta * (1.0f / distance);
	}
	else
	{
		// Sphere center is inside the box: push out along the axis with
		// the least penetration rather than leaving the contact undefined.
		px = half_extents.x() - std::fabs(local.x());
		py = half_extents.y() - std::fabs(local.y());
		pz = half_extents.z() - std::fabs(local.z());
		contact.valid = true;
		if (px <= py && px <= pz)
		{
			contact.penetration = px + radius;
			contact.normal = vec3(local.x() < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
		}
		else if (py <= pz)
		{
			contact.penetration = py + radius;
			contact.normal = vec3(0.0f, local.y() < 0.0f ? -1.0f : 1.0f, 0.0f);
		}
		else
		{
			contact.penetration = pz + radius;
			contact.normal = vec3(0.0f, 0.0f, local.z() < 0.0f ? -1.0f : 1.0f);
		}
	}
	return (contact);
}

} // namespace vre
