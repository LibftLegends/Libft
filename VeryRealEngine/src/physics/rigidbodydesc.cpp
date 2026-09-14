#include "rigidbodydesc.hpp"

namespace vre
{
RigidBodyDesc::RigidBodyDesc() : _mass(1.0f), _restitution(0.3f),
	_friction(0.5f), _gravity_scale(1.0f), _is_static(false), _is_trigger(false)
{
}

RigidBodyDesc::RigidBodyDesc(const RigidBodyDesc &other) :
	_position(other._position), _velocity(other._velocity), _mass(other._mass),
	_restitution(other._restitution), _friction(other._friction),
	_gravity_scale(other._gravity_scale), _is_static(other._is_static),
	_is_trigger(other._is_trigger), _collider(other._collider),
	_debug_name(other._debug_name)
{
}

RigidBodyDesc &RigidBodyDesc::operator=(const RigidBodyDesc &other)
{
	if (this != &other)
	{
		_position = other._position;
		_velocity = other._velocity;
		_mass = other._mass;
		_restitution = other._restitution;
		_friction = other._friction;
		_gravity_scale = other._gravity_scale;
		_is_static = other._is_static;
		_is_trigger = other._is_trigger;
		_collider = other._collider;
		_debug_name = other._debug_name;
	}
	return (*this);
}

RigidBodyDesc::~RigidBodyDesc()
{
}

const vec3 &RigidBodyDesc::position() const
{
	return (_position);
}

void RigidBodyDesc::set_position(const vec3 &value)
{
	_position = value;
}

const vec3 &RigidBodyDesc::velocity() const
{
	return (_velocity);
}

void RigidBodyDesc::set_velocity(const vec3 &value)
{
	_velocity = value;
}

float RigidBodyDesc::mass() const
{
	return (_mass);
}

void RigidBodyDesc::set_mass(float value)
{
	_mass = value;
}

float RigidBodyDesc::restitution() const
{
	return (_restitution);
}

void RigidBodyDesc::set_restitution(float value)
{
	_restitution = value;
}

float RigidBodyDesc::friction() const
{
	return (_friction);
}

void RigidBodyDesc::set_friction(float value)
{
	_friction = value;
}

float RigidBodyDesc::gravity_scale() const
{
	return (_gravity_scale);
}

void RigidBodyDesc::set_gravity_scale(float value)
{
	_gravity_scale = value;
}

bool RigidBodyDesc::is_static() const
{
	return (_is_static);
}

void RigidBodyDesc::set_static(bool value)
{
	_is_static = value;
}

bool RigidBodyDesc::is_trigger() const
{
	return (_is_trigger);
}

void RigidBodyDesc::set_trigger(bool value)
{
	_is_trigger = value;
}

const Collider &RigidBodyDesc::collider() const
{
	return (_collider);
}

Collider &RigidBodyDesc::collider()
{
	return (_collider);
}

const std::string &RigidBodyDesc::debug_name() const
{
	return (_debug_name);
}

void RigidBodyDesc::set_debug_name(const std::string &value)
{
	_debug_name = value;
}

} // namespace vre
