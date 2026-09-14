/**
 * @file transformcomponent.hpp
 * @brief An entity's local (parent-relative) transform, as authored in the
 * scene file plus any runtime spin.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{
class TransformComponent
{
  public:
	TransformComponent();
	TransformComponent(const TransformComponent &other);
	TransformComponent &operator=(const TransformComponent &other);
	~TransformComponent();

	const vec3 &position() const;
	void set_position(const vec3 &value);

	/** Static base rotation, radians (from the scene file). */
	const vec3 &rotation() const;
	void set_rotation(const vec3 &value);

	const vec3 &scale() const;
	void set_scale(const vec3 &value);

	/** Continuous rotation speed, radians/second (optional, default 0). */
	const vec3 &spin() const;
	void set_spin(const vec3 &value);

	const vec3 &spin_accumulated() const;
	void set_spin_accumulated(const vec3 &value);

  private:
	vec3 _position;
	vec3 _rotation;
	vec3 _scale;
	vec3 _spin;
	vec3 _spin_accumulated;
};

} // namespace vre
