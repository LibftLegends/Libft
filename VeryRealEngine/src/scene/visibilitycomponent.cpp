#include "visibilitycomponent.hpp"

namespace vre
{
VisibilityComponent::VisibilityComponent() : _visible(true)
{
}

VisibilityComponent::VisibilityComponent(
	const VisibilityComponent &other) : _visible(other._visible)
{
}

VisibilityComponent &VisibilityComponent::operator=(
	const VisibilityComponent &other)
{
	if (this != &other)
		_visible = other._visible;
	return (*this);
}

VisibilityComponent::~VisibilityComponent()
{
}

VisibilityComponent::VisibilityComponent(bool visible) : _visible(visible)
{
}

bool VisibilityComponent::visible() const
{
	return (_visible);
}

void VisibilityComponent::set_visible(bool value)
{
	_visible = value;
}

} // namespace vre
