#include "application.hpp"

namespace vre
{

void Application::handle_door_interaction(float delta_seconds)
{
	vec3	door_hinge_position;
	vec3	to_door;
	float	distance;
	float	door_target_angle;

	if (_scene.get_node_position("door_hinge", &door_hinge_position))
	{
		to_door = door_hinge_position - _camera.position();
		distance = std::sqrt(vec3::dot(to_door, to_door));
		if (distance <= kInteractRadius
			&& _window->was_key_pressed(Window::Key::E))
		{
			_door_open = !_door_open;
			std::fprintf(stderr, "Door %s.\n",
				_door_open ? "opened" : "closed");
			if (_door_creak_sound.is_valid())
				_audio->play(_door_creak_sound, /*loop=*/false,
					/*volume=*/0.8f);
		}
	}
	door_target_angle = _door_open ? _door_open_angle : 0.0f;
	_door_angle += (door_target_angle - _door_angle) * std::min(1.0f,
			delta_seconds * 5.0f);
	_scene.set_node_rotation("door_hinge", vec3(0.0f, _door_angle, 0.0f));
}

void Application::handle_light_switch_interaction()
{
	vec3	switch_position;
	vec3	to_switch;
	float	distance;

	if (!_scene.get_node_position("light_switch", &switch_position))
		return ;
	to_switch = switch_position - _camera.position();
	distance = std::sqrt(vec3::dot(to_switch, to_switch));
	if (distance <= kInteractRadius && _window->was_key_pressed(Window::Key::F))
	{
		_room_b_light_on = !_room_b_light_on;
		_scene.set_light_intensity(kRoomBLightIndex,
			_room_b_light_on ? kRoomBLightOnIntensity : kRoomBLightOffIntensity);
		std::fprintf(stderr, "Room B light %s.\n",
			_room_b_light_on ? "on" : "off");
		if (_click_sound.is_valid())
			_audio->play(_click_sound, /*loop=*/false, /*volume=*/0.6f);
	}
}

} // namespace vre
