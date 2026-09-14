#include "collider.hpp"

namespace vre
{
Collider::Collider() : _type(Type::Box), _half_extents(0.5f, 0.5f, 0.5f),
	_radius(0.5f)
{
}

Collider::Collider(const Collider &other) : _type(other._type),
	_half_extents(other._half_extents), _radius(other._radius)
{
}

Collider &Collider::operator=(const Collider &other)
{
	if (this != &other)
	{
		_type = other._type;
		_half_extents = other._half_extents;
		_radius = other._radius;
	}
	return (*this);
}

Collider::~Collider()
{
}

Collider::Type Collider::type() const
{
	return (_type);
}

void Collider::set_type(Type value)
{
	_type = value;
}

const vec3 &Collider::half_extents() const
{
	return (_half_extents);
}

void Collider::set_half_extents(const vec3 &value)
{
	_half_extents = value;
}

float Collider::radius() const
{
	return (_radius);
}

void Collider::set_radius(float value)
{
	_radius = value;
}

} // namespace vre
