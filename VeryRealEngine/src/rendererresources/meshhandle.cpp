#include "meshhandle.hpp"

namespace vre
{
MeshHandle::MeshHandle() : _value(invalid()._value)
{
}

MeshHandle::MeshHandle(const MeshHandle &other) : _value(other._value)
{
}

MeshHandle &MeshHandle::operator=(const MeshHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

MeshHandle::~MeshHandle()
{
}

MeshHandle::MeshHandle(size_t value) : _value(value)
{
}

size_t MeshHandle::value() const
{
	return (_value);
}

bool MeshHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool MeshHandle::operator==(const MeshHandle &other) const
{
	return (_value == other._value);
}

bool MeshHandle::operator!=(const MeshHandle &other) const
{
	return (_value != other._value);
}

MeshHandle MeshHandle::invalid()
{
	return (MeshHandle(static_cast<size_t>(-1)));
}

} // namespace vre
