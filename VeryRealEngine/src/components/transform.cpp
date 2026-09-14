#include "transform.hpp"

namespace vre
{
TransformComponent::TransformComponent() : _scale(1.0f, 1.0f, 1.0f)
{
}

TransformComponent::TransformComponent(const TransformComponent &other) :
	_position(other._position), _rotation(other._rotation),
	_scale(other._scale), _spin(other._spin),
	_spin_accumulated(other._spin_accumulated)
{
}

TransformComponent &TransformComponent::operator=(
	const TransformComponent &other)
{
	if (this != &other)
	{
		_position = other._position;
		_rotation = other._rotation;
		_scale = other._scale;
		_spin = other._spin;
		_spin_accumulated = other._spin_accumulated;
	}
	return (*this);
}

TransformComponent::~TransformComponent()
{
}

const vec3 &TransformComponent::position() const
{
	return (_position);
}

void TransformComponent::set_position(const vec3 &value)
{
	_position = value;
}

const vec3 &TransformComponent::rotation() const
{
	return (_rotation);
}

void TransformComponent::set_rotation(const vec3 &value)
{
	_rotation = value;
}

const vec3 &TransformComponent::scale() const
{
	return (_scale);
}

void TransformComponent::set_scale(const vec3 &value)
{
	_scale = value;
}

const vec3 &TransformComponent::spin() const
{
	return (_spin);
}

void TransformComponent::set_spin(const vec3 &value)
{
	_spin = value;
}

const vec3 &TransformComponent::spin_accumulated() const
{
	return (_spin_accumulated);
}

void TransformComponent::set_spin_accumulated(const vec3 &value)
{
	_spin_accumulated = value;
}

} // namespace vre
