#include "name.hpp"

namespace vre
{
NameComponent::NameComponent()
{
}

NameComponent::NameComponent(const NameComponent &other) : _name(other._name)
{
}

NameComponent &NameComponent::operator=(const NameComponent &other)
{
	if (this != &other)
		_name = other._name;
	return (*this);
}

NameComponent::~NameComponent()
{
}

NameComponent::NameComponent(const std::string &name) : _name(name)
{
}

const std::string &NameComponent::name() const
{
	return (_name);
}

void NameComponent::set_name(const std::string &value)
{
	_name = value;
}

} // namespace vre
