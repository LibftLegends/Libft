#include "physicsbody.hpp"

namespace vre
{
PhysicsBodyComponent::PhysicsBodyComponent() : _body(BodyHandle::invalid())
{
}

PhysicsBodyComponent::PhysicsBodyComponent(
	const PhysicsBodyComponent &other) : _body(other._body)
{
}

PhysicsBodyComponent &PhysicsBodyComponent::operator=(
	const PhysicsBodyComponent &other)
{
	if (this != &other)
		_body = other._body;
	return (*this);
}

PhysicsBodyComponent::~PhysicsBodyComponent()
{
}

PhysicsBodyComponent::PhysicsBodyComponent(BodyHandle body) : _body(body)
{
}

BodyHandle PhysicsBodyComponent::body() const
{
	return (_body);
}

void PhysicsBodyComponent::set_body(BodyHandle value)
{
	_body = value;
}

} // namespace vre
