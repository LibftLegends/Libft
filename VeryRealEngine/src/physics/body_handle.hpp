/**
 * @file body_handle.hpp
 * @brief Opaque handle identifying a rigid body inside a PhysicsWorld.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class BodyHandle
{
  public:
	BodyHandle();
	BodyHandle(const BodyHandle &other);
	BodyHandle &operator=(const BodyHandle &other);
	~BodyHandle();

	explicit BodyHandle(size_t value);

	size_t value() const;
	bool is_valid() const;
	bool operator==(const BodyHandle &other) const;
	bool operator!=(const BodyHandle &other) const;

	/// @return The sentinel meaning "no body".
	static BodyHandle invalid();

  private:
	size_t _value;
};

} // namespace vre
