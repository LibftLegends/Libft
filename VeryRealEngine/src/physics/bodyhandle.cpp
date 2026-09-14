#include "bodyhandle.hpp"

namespace vre
{
BodyHandle::BodyHandle() : _value(invalid()._value)
{
}

BodyHandle::BodyHandle(const BodyHandle &other) : _value(other._value)
{
}

BodyHandle &BodyHandle::operator=(const BodyHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

BodyHandle::~BodyHandle()
{
}

BodyHandle::BodyHandle(size_t value) : _value(value)
{
}

size_t BodyHandle::value() const
{
	return (_value);
}

bool BodyHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool BodyHandle::operator==(const BodyHandle &other) const
{
	return (_value == other._value);
}

bool BodyHandle::operator!=(const BodyHandle &other) const
{
	return (_value != other._value);
}

BodyHandle BodyHandle::invalid()
{
	return (BodyHandle(static_cast<size_t>(-1)));
}

} // namespace vre
