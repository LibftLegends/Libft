#include "material_handle.hpp"

namespace vre
{

MaterialHandle::MaterialHandle() : _value(invalid()._value)
{
}

MaterialHandle::MaterialHandle(const MaterialHandle &other) : _value(other._value)
{
}

MaterialHandle &MaterialHandle::operator=(const MaterialHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

MaterialHandle::~MaterialHandle()
{
}

MaterialHandle::MaterialHandle(size_t value) : _value(value)
{
}

size_t MaterialHandle::value() const
{
	return (_value);
}

bool MaterialHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool MaterialHandle::operator==(const MaterialHandle &other) const
{
	return (_value == other._value);
}

bool MaterialHandle::operator!=(const MaterialHandle &other) const
{
	return (_value != other._value);
}

MaterialHandle MaterialHandle::invalid()
{
	return (MaterialHandle(static_cast<size_t>(-1)));
}

} // namespace vre
