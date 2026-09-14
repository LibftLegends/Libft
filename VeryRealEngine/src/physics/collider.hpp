/**
 * @file collider.hpp

	* @brief A rigid body's collision shape: either an axis-aligned box or a sphere.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{

class Collider
{
  public:
	/// Which shape a Collider represents.
	enum class Type
	{
		Box,    ///< Axis-aligned, defined by half_extents().
		Sphere, ///< Defined by radius().
	};

	Collider();
	Collider(const Collider &other);
	Collider &operator=(const Collider &other);
	~Collider();

	Type type() const;
	void set_type(Type value);

	/** Used when type() == Box. */
	const vec3 &half_extents() const;
	void set_half_extents(const vec3 &value);

	/** Used when type() == Sphere. */
	float radius() const;
	void set_radius(float value);

  private:
	Type _type;
	vec3 _half_extents;
	float _radius;
};

} // namespace vre
