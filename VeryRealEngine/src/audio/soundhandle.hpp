/**
 * @file soundhandle.hpp
 * @brief Opaque handle to a loaded sound clip (returned by
 * AudioSystem::load_sound).
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class SoundHandle
{
  public:
	SoundHandle();
	SoundHandle(const SoundHandle &other);
	SoundHandle &operator=(const SoundHandle &other);
	~SoundHandle();

	explicit SoundHandle(uint32_t value);

	uint32_t value() const;
	bool is_valid() const;
	bool operator==(const SoundHandle &other) const;
	bool operator!=(const SoundHandle &other) const;

	/// @return The sentinel meaning "no sound"/"load failed".
	static SoundHandle invalid();

  private:
	uint32_t _value;
};

} // namespace vre
