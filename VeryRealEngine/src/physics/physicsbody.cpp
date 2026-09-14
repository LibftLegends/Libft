#include "physicsbody.hpp"

namespace vre
{
PhysicsBody::PhysicsBody() : inverse_mass(0.0f), restitution(0.0f),
	friction(0.0f), gravity_scale(1.0f), is_static(false), is_trigger(false)
{
}

PhysicsBody::PhysicsBody(const PhysicsBody &other) : position(other.position),
	velocity(other.velocity), inverse_mass(other.inverse_mass),
	restitution(other.restitution), friction(other.friction),
	gravity_scale(other.gravity_scale), is_static(other.is_static),
	is_trigger(other.is_trigger), collider(other.collider),
	debug_name(other.debug_name)
{
}

PhysicsBody &PhysicsBody::operator=(const PhysicsBody &other)
{
	if (this != &other)
	{
		position = other.position;
		velocity = other.velocity;
		inverse_mass = other.inverse_mass;
		restitution = other.restitution;
		friction = other.friction;
		gravity_scale = other.gravity_scale;
		is_static = other.is_static;
		is_trigger = other.is_trigger;
		collider = other.collider;
		debug_name = other.debug_name;
	}
	return (*this);
}

PhysicsBody::~PhysicsBody()
{
}

} // namespace vre
