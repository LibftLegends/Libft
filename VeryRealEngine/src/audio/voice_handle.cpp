#include "voice_handle.hpp"

namespace vre
{

VoiceHandle::VoiceHandle() : _value(invalid()._value)
{
}

VoiceHandle::VoiceHandle(const VoiceHandle &other) : _value(other._value)
{
}

VoiceHandle &VoiceHandle::operator=(const VoiceHandle &other)
{
	if (this != &other)
		_value = other._value;
	return (*this);
}

VoiceHandle::~VoiceHandle()
{
}

VoiceHandle::VoiceHandle(uint32_t value) : _value(value)
{
}

uint32_t VoiceHandle::value() const
{
	return (_value);
}

bool VoiceHandle::is_valid() const
{
	return (_value != invalid()._value);
}

bool VoiceHandle::operator==(const VoiceHandle &other) const
{
	return (_value == other._value);
}

bool VoiceHandle::operator!=(const VoiceHandle &other) const
{
	return (_value != other._value);
}

VoiceHandle VoiceHandle::invalid()
{
	return (VoiceHandle(static_cast<uint32_t>(-1)));
}

} // namespace vre
