#include "entity.hpp"

namespace vre::ecs
{
Entity::Entity() : _value(0)
{
}

Entity::Entity(const Entity &other) : _value(other._value)
{
}

Entity &Entity::operator=(const Entity &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

Entity::~Entity()
{
}

Entity::Entity(uint32_t value) : _value(value)
{
}

uint32_t Entity::value() const
{
	return (_value);
}

bool Entity::is_valid() const
{
	return (_value != invalid()._value);
}

bool Entity::operator==(const Entity &other) const
{
	return (_value == other._value);
}

bool Entity::operator!=(const Entity &other) const
{
	return (_value != other._value);
}

Entity Entity::invalid()
{
	return (Entity(static_cast<uint32_t>(-1)));
}

} // namespace vre::ecs
