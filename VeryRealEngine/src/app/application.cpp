#include "application.hpp"

namespace vre
{

Application::Application() : _window(nullptr), _is_house_scene(false),
	_pendulum_animator(nullptr, nullptr), _pendulum_model(), _audio(nullptr),
	_audio_available(false), _ambient_sound(SoundHandle::invalid()),
	_click_sound(SoundHandle::invalid()),
	_door_creak_sound(SoundHandle::invalid()), _door_open(false),
	_door_angle(0.0f), _door_open_angle(1.35f), _room_b_light_on(false),
	_auto_yaw_speed(0.0f), _screenshot_path(nullptr),
	_screenshot_after_frame(60), _frame_counter(0), _exit_after_frame(0),
	_physics_accumulator(0.0f), _frames_this_window(0),
	_fps_report_accumulator(0.0f)
{
}

Application::~Application()
{
}

int Application::run(const char *scene_path)
{
	if (!initialize(scene_path))
		return (1);
	main_loop();
	shutdown();
	return (0);
}

} // namespace vre
