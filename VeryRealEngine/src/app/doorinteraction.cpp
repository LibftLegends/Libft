#include "doorinteraction.hpp"

namespace vre
{
DoorInteraction::DoorInteraction() : _open(false), _angle(0.0f),
	_open_angle(1.35f)
{
}

DoorInteraction::DoorInteraction(const DoorInteraction &other) :
	_open(other._open), _angle(other._angle), _open_angle(other._open_angle)
{
}

DoorInteraction &DoorInteraction::operator=(const DoorInteraction &other)
{
	if (this != &other)
	{
		_open = other._open;
		_angle = other._angle;
		_open_angle = other._open_angle;
	}
	return (*this);
}

DoorInteraction::~DoorInteraction()
{
}

bool DoorInteraction::is_open() const
{
	return (_open);
}

void DoorInteraction::set_open_angle(float value)
{
	_open_angle = value;
}

void DoorInteraction::force_open()
{
	_open = true;
	_angle = _open_angle; // skip the lerp-open animation entirely
}

void DoorInteraction::update(float delta_seconds, Scene *scene,
	Window *window, const vec3 &camera_position, DemoAudio *audio)
{
	vec3	hinge_position;
	vec3	to_door;
	float	distance;
	float	target_angle;

	if (scene->get_node_position("door_hinge", &hinge_position))
	{
		to_door = hinge_position - camera_position;
		distance = std::sqrt(vec3::dot(to_door, to_door));
		if (distance <= kInteractRadius
			&& window->was_key_pressed(Window::Key::E))
		{
			_open = !_open;
			std::fprintf(stderr, "Door %s.\n", _open ? "opened" : "closed");
			audio->play_door_creak();
		}
	}
	target_angle = _open ? _open_angle : 0.0f;
	_angle += (target_angle - _angle) * std::min(1.0f, delta_seconds * 5.0f);
	scene->set_node_rotation("door_hinge", vec3(0.0f, _angle, 0.0f));
}

} // namespace vre
