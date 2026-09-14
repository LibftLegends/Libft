/**
 * @file debugconfig.hpp
 * @brief Debug/verification overrides, read from the environment rather
 * than argv: this is purely for scripted screenshot capture in a context
 * with no keyboard/mouse to actually drive the demo interactively — e.g.
 * positioning the camera to look at a specific object, or forcing the
 * door/light into a particular state without waiting for real input. None
 * of this affects normal interactive use: every env var is optional and
 * only read once, at construction.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../scene/scene.hpp"
#include "../vre.hpp"
#include "doorinteraction.hpp"
#include "firstpersoncamera.hpp"
#include "lightswitchinteraction.hpp"

namespace vre
{
class DebugConfig
{
  public:
	DebugConfig();
	DebugConfig(const DebugConfig &other);
	DebugConfig &operator=(const DebugConfig &other);
	~DebugConfig();

	/// Applies VRE_CAMERA_POS/_YAW/_PITCH, if set, onto `camera`.
	void apply_camera(FirstPersonCamera *camera) const;
	/// Applies VRE_DOOR_OPEN_ANGLE/VRE_FORCE_DOOR_OPEN, if set, onto `door`.
	void apply_door(DoorInteraction *door) const;
	/// Applies VRE_FORCE_LIGHT_ON, if set, onto `light_switch`.
	void apply_light_switch(LightSwitchInteraction *light_switch) const;
	/// Applies VRE_FORCE_HIDE_NODE, if set, onto `scene`.
	void apply_scene(Scene *scene) const;

	/// Continuous camera auto-pan speed, radians/sec
	/// (VRE_AUTO_YAW_SPEED, default 0).
	float auto_yaw_speed() const;
	/// Screenshot destination path (VRE_SCREENSHOT_PATH), or nullptr
	/// if unset.
	const char *screenshot_path() const;
	/// Frame index to capture the screenshot at (VRE_SCREENSHOT_FRAME,
	/// default 60).
	uint64_t screenshot_after_frame() const;
	/// Frame count to exit after (VRE_EXIT_AFTER_FRAME), or 0 to run forever.
	uint64_t exit_after_frame() const;

  private:
	bool _has_camera_position;
	vec3 _camera_position;
	bool _has_camera_yaw;
	float _camera_yaw;
	bool _has_camera_pitch;
	float _camera_pitch;
	bool _has_door_open_angle;
	float _door_open_angle;
	bool _force_door_open;
	bool _force_light_on;
	bool _has_force_hide_node;
	std::string _force_hide_node;
	float _auto_yaw_speed;
	const char *_screenshot_path;
	uint64_t _screenshot_after_frame;
	uint64_t _exit_after_frame;
};

} // namespace vre
