/**
 * @file firstpersoncamera.hpp
 * @brief WASD/arrow-key first-person camera: player position, yaw/pitch,
 * and the view matrix + screen-space pan velocity derived from them.
 */
#pragma once

#include "../math/mat4.hpp"
#include "../math/vec3.hpp"
#include "../platform/window.hpp"
#include "../vre.hpp"

namespace vre
{
class FirstPersonCamera
{
  public:
	FirstPersonCamera();
	FirstPersonCamera(const FirstPersonCamera &other);
	FirstPersonCamera &operator=(const FirstPersonCamera &other);
	~FirstPersonCamera();

	const vec3 &position() const;
	void set_position(const vec3 &value);
	float yaw() const;
	void set_yaw(float value);
	float pitch() const;
	void set_pitch(float value);

	/**
		* @brief Applies one frame's arrow-key look input, WASD movement, and
		* a constant auto-yaw pan, remembering the pre-look yaw/pitch for the
		* motion-blur velocity computed by screen_space_velocity_x()/y().
		*/
	void update(const Window &window, float delta_seconds,
		float auto_yaw_speed);

	/// @return The view matrix looking from position() along the
	/// current yaw/pitch.
	mat4 view_matrix() const;

	/**
		* @brief This frame's horizontal look delta as a fraction of the
		* camera's horizontal field of view (see post.frag's motion-blur
		* comment).
		*/
	float screen_space_velocity_x(float vertical_fov, float aspect) const;
	/// Same, vertical component, as a fraction of the vertical field of view.
	float screen_space_velocity_y(float vertical_fov) const;

  private:
	/// Clamps pitch so the player can't flip the camera past straight up/down.
	static float clamp_pitch(float pitch);

	void apply_look_input(const Window &window, float delta_seconds,
		float auto_yaw_speed);
	void apply_move_input(const Window &window, float delta_seconds);

	vec3 _position;
	float _yaw;
	float _pitch;
	float _yaw_before_look;
	float _pitch_before_look;
	float _move_speed;
	float _look_speed;
};

} // namespace vre
