/**
 * @file physics_resolve.cpp
 * @brief PhysicsWorld::resolve_contact()'s body, split out of
 * physics_world.cpp purely to keep each file under this project's
 * 250-line cap — both files define methods of the same PhysicsWorld class
 * declared in physics_world.hpp.
 */
#include "physics_world.hpp"

namespace vre
{

void PhysicsWorld::resolve_contact(Body &a, Body &b, const Contact &contact)
{
	float		total_inverse_mass;
	const float	correction_percent = 0.2f;
	const float	slop = 0.01f;
	float		correction_magnitude;
	vec3		correction;
	vec3		relative_velocity;
	float		velocity_along_normal;
	float		restitution;
	float		normal_impulse_magnitude;
	vec3		normal_impulse;
	float		remaining_normal_component;
	vec3		tangent_velocity;
	float		tangent_speed;
	vec3		tangent;
	float		tangent_impulse_magnitude;
	float		mu;
	float		max_friction_impulse;
	vec3		friction_impulse;

	total_inverse_mass = a.inverse_mass + b.inverse_mass;
	if (total_inverse_mass <= 0.0f)
		return ; // both static (or both zero-mass) — nothing to push apart
	// Positional correction: pull the bodies apart along the contact normal
	// proportional to their share of inverse mass, leaving a small "slop"
	// so resting contacts don't jitter as correction and gravity fight.
	correction_magnitude = std::max(contact.penetration - slop, 0.0f)
		/ total_inverse_mass * correction_percent;
	correction = contact.normal * correction_magnitude;
	a.position = a.position - correction * a.inverse_mass;
	b.position = b.position + correction * b.inverse_mass;
	// Impulse-based velocity response along the normal (restitution).
	relative_velocity = b.velocity - a.velocity;
	velocity_along_normal = vec3::dot(relative_velocity, contact.normal);
	if (velocity_along_normal > 0.0f)
		return ; // already separating, no impulse needed
	restitution = std::min(a.restitution, b.restitution);
	normal_impulse_magnitude = -(1.0f + restitution) * velocity_along_normal
		/ total_inverse_mass;
	normal_impulse = contact.normal * normal_impulse_magnitude;
	a.velocity = a.velocity - normal_impulse * a.inverse_mass;
	b.velocity = b.velocity + normal_impulse * b.inverse_mass;
	// Coulomb friction: a tangential impulse opposing relative sliding,
	// capped by mu * normal impulse so friction never accelerates sliding.
	relative_velocity = b.velocity - a.velocity;
	remaining_normal_component = vec3::dot(relative_velocity, contact.normal);
	tangent_velocity = relative_velocity - contact.normal
		* remaining_normal_component;
	tangent_speed = std::sqrt(vec3::dot(tangent_velocity, tangent_velocity));
	if (tangent_speed > 1e-6f)
	{
		tangent = tangent_velocity * (1.0f / tangent_speed);
		tangent_impulse_magnitude = -vec3::dot(relative_velocity, tangent)
			/ total_inverse_mass;
		mu = std::sqrt(a.friction * b.friction);
		max_friction_impulse = mu * normal_impulse_magnitude;
		tangent_impulse_magnitude = std::clamp(tangent_impulse_magnitude,
				-max_friction_impulse, max_friction_impulse);
		friction_impulse = tangent * tangent_impulse_magnitude;
		a.velocity = a.velocity - friction_impulse * a.inverse_mass;
		b.velocity = b.velocity + friction_impulse * b.inverse_mass;
	}
}

} // namespace vre
