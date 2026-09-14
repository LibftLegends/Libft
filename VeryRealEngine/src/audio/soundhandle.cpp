#include "soundhandle.hpp"

namespace vre
{
SoundHandle::SoundHandle() : _value(invalid()._value)
{
}

SoundHandle::SoundHandle(const SoundHandle &other) : _value(other._value)
{
}

SoundHandle &SoundHandle::operator=(const SoundHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

SoundHandle::~SoundHandle()
{
}

SoundHandle::SoundHandle(uint32_t value) : _value(value)
{
}

uint32_t SoundHandle::value() const
{
	return (_value);
}

bool SoundHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool SoundHandle::operator==(const SoundHandle &other) const
{
	return (_value == other._value);
}

bool SoundHandle::operator!=(const SoundHandle &other) const
{
	return (_value != other._value);
}

SoundHandle SoundHandle::invalid()
{
	return (SoundHandle(static_cast<uint32_t>(-1)));
}

} // namespace vre
