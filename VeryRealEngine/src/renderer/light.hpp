/**
 * @file light.hpp
 * @brief A single scene light: directional or point, with color/intensity
 * and, for the first VulkanRenderer-configured shadow casters (in scene
 * order), a real-time shadow map.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{
class Light
{
  public:
	/// Distinguishes how direction_or_position() should be interpreted.
	enum class Type
	{
		Directional,
			///< Parallel rays (e.g. sunlight); direction_or_position()
			///< is a unit direction.
		Point,
			///< Radiates from a point; direction_or_position() is a
			///< world-space position.
	};

	Light();
	Light(const Light &other);
	Light &operator=(const Light &other);
	~Light();

	Type type() const;
	void set_type(Type value);

	/**
		* Directional: unit direction the light travels (e.g. (0,-1,0) =
		* straight down).
		* Point: world-space position.
		*/
	const vec3 &direction_or_position() const;
	void set_direction_or_position(const vec3 &value);

	/** Linear RGB color, multiplied by intensity(). */
	const vec3 &color() const;
	void set_color(const vec3 &value);

	/** Scalar multiplier applied to color(). */
	float intensity() const;
	void set_intensity(float value);

  private:
	Type _type;
	vec3 _direction_or_position;
	vec3 _color;
	float _intensity;
};

} // namespace vre
