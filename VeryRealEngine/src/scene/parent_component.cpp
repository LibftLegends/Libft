#include "parent_component.hpp"

namespace vre
{

ParentComponent::ParentComponent() : _parent(ecs::Entity::invalid())
{
}

ParentComponent::ParentComponent(const ParentComponent &other) : _parent(other._parent)
{
}

ParentComponent &ParentComponent::operator=(const ParentComponent &other)
{
	if (this != &other)
		_parent = other._parent;
	return (*this);
}

ParentComponent::~ParentComponent()
{
}

ParentComponent::ParentComponent(ecs::Entity parent) : _parent(parent)
{
}

ecs::Entity ParentComponent::parent() const
{
	return (_parent);
}

void ParentComponent::set_parent(ecs::Entity value)
{
	_parent = value;
}

} // namespace vre
