#include "world_transform_component.hpp"

namespace vre
{

WorldTransformComponent::WorldTransformComponent() : _value(mat4::identity())
{
}

WorldTransformComponent::WorldTransformComponent(const WorldTransformComponent &other) : _value(other._value)
{
}

WorldTransformComponent &WorldTransformComponent::operator=(const WorldTransformComponent &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

WorldTransformComponent::~WorldTransformComponent()
{
}

WorldTransformComponent::WorldTransformComponent(const mat4 &value) : _value(value)
{
}

const mat4 &WorldTransformComponent::value() const
{
	return (_value);
}

void WorldTransformComponent::set_value(const mat4 &value)
{
	_value = value;
}

} // namespace vre
