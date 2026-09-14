#include "first_person_camera.hpp"

namespace vre
{

FirstPersonCamera::FirstPersonCamera() : _position(-4.0f, 1.6f, 0.5f),
	_yaw(1.3f), _pitch(-0.08f), _yaw_before_look(_yaw),
	_pitch_before_look(_pitch), _move_speed(2.5f), _look_speed(2.0f)
{
}

FirstPersonCamera::FirstPersonCamera(const FirstPersonCamera &other) : _position(other._position),
	_yaw(other._yaw), _pitch(other._pitch),
	_yaw_before_look(other._yaw_before_look),
	_pitch_before_look(other._pitch_before_look),
	_move_speed(other._move_speed), _look_speed(other._look_speed)
{
}

FirstPersonCamera &FirstPersonCamera::operator=(const FirstPersonCamera &other)
{
	if (this != &other)
	{
		_position = other._position;
		_yaw = other._yaw;
		_pitch = other._pitch;
		_yaw_before_look = other._yaw_before_look;
		_pitch_before_look = other._pitch_before_look;
		_move_speed = other._move_speed;
		_look_speed = other._look_speed;
	}
	return (*this);
}

FirstPersonCamera::~FirstPersonCamera()
{
}

const vec3 &FirstPersonCamera::position() const
{
	return (_position);
}

void FirstPersonCamera::set_position(const vec3 &value)
{
	_position = value;
}

float FirstPersonCamera::yaw() const
{
	return (_yaw);
}

void FirstPersonCamera::set_yaw(float value)
{
	_yaw = value;
}

float FirstPersonCamera::pitch() const
{
	return (_pitch);
}

void FirstPersonCamera::set_pitch(float value)
{
	_pitch = value;
}

float FirstPersonCamera::clamp_pitch(float pitch)
{
	const float limit = 1.4f; // radians, just under 90 degrees
	if (pitch > limit)
		return (limit);
	if (pitch < -limit)
		return (-limit);
	return (pitch);
}

void FirstPersonCamera::apply_look_input(const Window &window,
	float delta_seconds, float auto_yaw_speed)
{
	_yaw_before_look = _yaw;
	_pitch_before_look = _pitch;
	if (window.is_key_held(Window::Key::Left))
		_yaw -= _look_speed * delta_seconds;
	if (window.is_key_held(Window::Key::Right))
		_yaw += _look_speed * delta_seconds;
	if (window.is_key_held(Window::Key::Up))
		_pitch = clamp_pitch(_pitch + _look_speed * delta_seconds);
	if (window.is_key_held(Window::Key::Down))
		_pitch = clamp_pitch(_pitch - _look_speed * delta_seconds);
	_yaw += auto_yaw_speed * delta_seconds;
}

void FirstPersonCamera::apply_move_input(const Window &window,
	float delta_seconds)
{
	vec3 flat_forward(std::sin(_yaw), 0.0f, -std::cos(_yaw));
	vec3 flat_right(std::cos(_yaw), 0.0f, std::sin(_yaw));
	if (window.is_key_held(Window::Key::W))
		_position = _position + flat_forward * (_move_speed * delta_seconds);
	if (window.is_key_held(Window::Key::S))
		_position = _position - flat_forward * (_move_speed * delta_seconds);
	if (window.is_key_held(Window::Key::D))
		_position = _position + flat_right * (_move_speed * delta_seconds);
	if (window.is_key_held(Window::Key::A))
		_position = _position - flat_right * (_move_speed * delta_seconds);
}

void FirstPersonCamera::update(const Window &window, float delta_seconds,
	float auto_yaw_speed)
{
	apply_look_input(window, delta_seconds, auto_yaw_speed);
	apply_move_input(window, delta_seconds);
}

mat4 FirstPersonCamera::view_matrix() const
{
	vec3 look_forward(std::sin(_yaw) * std::cos(_pitch), std::sin(_pitch),
		-std::cos(_yaw) * std::cos(_pitch));

	return (mat4::look_at(_position, _position + look_forward, vec3(0.0f, 1.0f,
				0.0f)));
}

float FirstPersonCamera::screen_space_velocity_x(float vertical_fov,
	float aspect) const
{
	float horizontal_fov;

	horizontal_fov = 2.0f * std::atan(std::tan(vertical_fov * 0.5f) * aspect);
	return ((_yaw - _yaw_before_look) / horizontal_fov);
}

float FirstPersonCamera::screen_space_velocity_y(float vertical_fov) const
{
	return ((_pitch - _pitch_before_look) / vertical_fov);
}

} // namespace vre
