#include "lightswitchinteraction.hpp"

namespace vre
{
LightSwitchInteraction::LightSwitchInteraction() : _on(false)
{
}

LightSwitchInteraction::LightSwitchInteraction(
	const LightSwitchInteraction &other) : _on(other._on)
{
}

LightSwitchInteraction &LightSwitchInteraction::operator=(
	const LightSwitchInteraction &other)
{
	if (this != &other)
		_on = other._on;
	return (*this);
}

LightSwitchInteraction::~LightSwitchInteraction()
{
}

bool LightSwitchInteraction::is_on() const
{
	return (_on);
}

void LightSwitchInteraction::set_on(bool value)
{
	_on = value;
}

void LightSwitchInteraction::apply_intensity(Scene *scene) const
{
	scene->set_light_intensity(kLightIndex,
			_on ? kOnIntensity : kOffIntensity);
}

void LightSwitchInteraction::update(Scene *scene, Window *window,
	const vec3 &camera_position, DemoAudio *audio)
{
	vec3	switch_position;
	vec3	to_switch;
	float	distance;

	if (!scene->get_node_position("light_switch", &switch_position))
		return ;
	to_switch = switch_position - camera_position;
	distance = std::sqrt(vec3::dot(to_switch, to_switch));
	if (distance <= kInteractRadius
		&& window->was_key_pressed(Window::Key::F))
	{
		_on = !_on;
		apply_intensity(scene);
		std::fprintf(stderr, "Room B light %s.\n", _on ? "on" : "off");
		audio->play_click();
	}
}

} // namespace vre
