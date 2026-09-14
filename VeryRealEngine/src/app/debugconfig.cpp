#include "debugconfig.hpp"

namespace vre
{
DebugConfig::DebugConfig() : _has_camera_position(false), _camera_position(),
	_has_camera_yaw(false), _camera_yaw(0.0f), _has_camera_pitch(false),
	_camera_pitch(0.0f), _has_door_open_angle(false), _door_open_angle(0.0f),
	_force_door_open(false), _force_light_on(false),
	_has_force_hide_node(false), _auto_yaw_speed(0.0f),
	_screenshot_path(std::getenv("VRE_SCREENSHOT_PATH")),
	_screenshot_after_frame(60), _exit_after_frame(0)
{
	float	x;
	float	y;
	float	z;

	if (const char *env = std::getenv("VRE_CAMERA_POS"))
	{
		if (std::sscanf(env, "%f,%f,%f", &x, &y, &z) == 3)
		{
			_has_camera_position = true;
			_camera_position = vec3(x, y, z);
		}
	}
	if (const char *env = std::getenv("VRE_CAMERA_YAW"))
	{
		_has_camera_yaw = true;
		_camera_yaw = static_cast<float>(std::atof(env));
	}
	if (const char *env = std::getenv("VRE_CAMERA_PITCH"))
	{
		_has_camera_pitch = true;
		_camera_pitch = static_cast<float>(std::atof(env));
	}
	// Overrides how far a forced door-open below opens the door — lets a
	// screenshot show a small, deliberate gap instead of only the full
	// ~77-degree swing.
	if (const char *env = std::getenv("VRE_DOOR_OPEN_ANGLE"))
	{
		_has_door_open_angle = true;
		_door_open_angle = static_cast<float>(std::atof(env));
	}
	_force_door_open = (std::getenv("VRE_FORCE_DOOR_OPEN") != nullptr);
	_force_light_on = (std::getenv("VRE_FORCE_LIGHT_ON") != nullptr);
	// Exercises the exact mechanism Chapter IV.1's "hide an object and all
	// its children simultaneously by editing only the parent" is evaluated
	// against (Scene::set_visible), without needing a live H-keypress.
	if (const char *env = std::getenv("VRE_FORCE_HIDE_NODE"))
	{
		_has_force_hide_node = true;
		_force_hide_node = env;
	}
	// Continuously pans the camera at a fixed angular speed (radians/sec) —
	// exists purely so the post-process motion-blur bonus effect can be
	// exercised and screenshotted without live keyboard input.
	if (const char *env = std::getenv("VRE_AUTO_YAW_SPEED"))
		_auto_yaw_speed = static_cast<float>(std::atof(env));
	if (const char *env = std::getenv("VRE_SCREENSHOT_FRAME"))
		_screenshot_after_frame = static_cast<uint64_t>(std::atoll(env));
	if (const char *env = std::getenv("VRE_EXIT_AFTER_FRAME"))
		_exit_after_frame = static_cast<uint64_t>(std::atoll(env));
}

DebugConfig::DebugConfig(const DebugConfig &other) :
	_has_camera_position(other._has_camera_position),
	_camera_position(other._camera_position),
	_has_camera_yaw(other._has_camera_yaw), _camera_yaw(other._camera_yaw),
	_has_camera_pitch(other._has_camera_pitch),
	_camera_pitch(other._camera_pitch),
	_has_door_open_angle(other._has_door_open_angle),
	_door_open_angle(other._door_open_angle),
	_force_door_open(other._force_door_open),
	_force_light_on(other._force_light_on),
	_has_force_hide_node(other._has_force_hide_node),
	_force_hide_node(other._force_hide_node),
	_auto_yaw_speed(other._auto_yaw_speed),
	_screenshot_path(other._screenshot_path),
	_screenshot_after_frame(other._screenshot_after_frame),
	_exit_after_frame(other._exit_after_frame)
{
}

DebugConfig &DebugConfig::operator=(const DebugConfig &other)
{
	if (this != &other)
	{
		_has_camera_position = other._has_camera_position;
		_camera_position = other._camera_position;
		_has_camera_yaw = other._has_camera_yaw;
		_camera_yaw = other._camera_yaw;
		_has_camera_pitch = other._has_camera_pitch;
		_camera_pitch = other._camera_pitch;
		_has_door_open_angle = other._has_door_open_angle;
		_door_open_angle = other._door_open_angle;
		_force_door_open = other._force_door_open;
		_force_light_on = other._force_light_on;
		_has_force_hide_node = other._has_force_hide_node;
		_force_hide_node = other._force_hide_node;
		_auto_yaw_speed = other._auto_yaw_speed;
		_screenshot_path = other._screenshot_path;
		_screenshot_after_frame = other._screenshot_after_frame;
		_exit_after_frame = other._exit_after_frame;
	}
	return (*this);
}

DebugConfig::~DebugConfig()
{
}

void DebugConfig::apply_camera(FirstPersonCamera *camera) const
{
	if (_has_camera_position)
		camera->set_position(_camera_position);
	if (_has_camera_yaw)
		camera->set_yaw(_camera_yaw);
	if (_has_camera_pitch)
		camera->set_pitch(_camera_pitch);
}

void DebugConfig::apply_door(DoorInteraction *door) const
{
	if (_has_door_open_angle)
		door->set_open_angle(_door_open_angle);
	if (_force_door_open)
		door->force_open();
}

void DebugConfig::apply_light_switch(
	LightSwitchInteraction *light_switch) const
{
	if (_force_light_on)
		light_switch->set_on(true);
}

void DebugConfig::apply_scene(Scene *scene) const
{
	if (_has_force_hide_node)
		scene->set_visible(_force_hide_node, false);
}

float DebugConfig::auto_yaw_speed() const
{
	return (_auto_yaw_speed);
}

const char *DebugConfig::screenshot_path() const
{
	return (_screenshot_path);
}

uint64_t DebugConfig::screenshot_after_frame() const
{
	return (_screenshot_after_frame);
}

uint64_t DebugConfig::exit_after_frame() const
{
	return (_exit_after_frame);
}

} // namespace vre
