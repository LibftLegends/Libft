/**
 * @file keyframe.hpp
 * @brief One bone's rotation at one point in time, in an AnimationTrack.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{

class Keyframe
{
  public:
	Keyframe();
	Keyframe(const Keyframe &other);
	Keyframe &operator=(const Keyframe &other);
	~Keyframe();

	float time() const;
	void set_time(float value);

	/** Euler radians,
		local (replaces the bone's bind_local_rotation at this time). */
	const vec3 &rotation() const;
	void set_rotation(const vec3 &value);

  private:
	float _time;
	vec3 _rotation;
};

} // namespace vre
