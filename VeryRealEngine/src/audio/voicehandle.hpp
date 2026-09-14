/**
 * @file voicehandle.hpp
 * @brief Opaque handle to one playing instance of a sound (returned by
 * AudioSystem::play).
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class VoiceHandle
{
  public:
	VoiceHandle();
	VoiceHandle(const VoiceHandle &other);
	VoiceHandle &operator=(const VoiceHandle &other);
	~VoiceHandle();

	explicit VoiceHandle(uint32_t value);

	uint32_t value() const;
	bool is_valid() const;
	bool operator==(const VoiceHandle &other) const;
	bool operator!=(const VoiceHandle &other) const;

	/// @return The sentinel meaning "no voice"/"play failed".
	static VoiceHandle invalid();

  private:
	uint32_t _value;
};

} // namespace vre
