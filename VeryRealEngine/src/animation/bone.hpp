/**
 * @file bone.hpp
 * @brief One bone's place in the hierarchy and its bind (rest) pose local
 * transform, relative to its parent.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{
class Bone
{
  public:
	Bone();
	Bone(const Bone &other);
	Bone &operator=(const Bone &other);
	~Bone();

	const std::string &name() const;
	void set_name(const std::string &value);

	int32_t parent_index() const;
	void set_parent_index(int32_t value);

	const vec3 &bind_local_position() const;
	void set_bind_local_position(const vec3 &value);

	/** Euler radians (X, Y, Z, applied Z*Y*X — see mat4::compose). */
	const vec3 &bind_local_rotation() const;
	void set_bind_local_rotation(const vec3 &value);

	/// @return The sentinel parent_index() meaning "no parent" (a root bone).
	static int32_t no_parent_index();

  private:
	std::string _name;
	int32_t _parent_index;
	vec3 _bind_local_position;
	vec3 _bind_local_rotation;
};

} // namespace vre
