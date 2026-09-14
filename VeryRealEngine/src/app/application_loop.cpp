#include "application.hpp"

namespace vre
{

void Application::main_loop()
{
	float	delta_seconds;

	std::chrono::high_resolution_clock::time_point last_time;
	std::chrono::high_resolution_clock::time_point now;
	last_time = std::chrono::high_resolution_clock::now();
	while (!_window->should_close())
	{
		_window->poll_events();
		if (_window->was_key_pressed(Window::Key::Escape))
			break ;
		now = std::chrono::high_resolution_clock::now();
		delta_seconds = std::chrono::duration<float>(now - last_time).count();
		last_time = now;
		update_and_draw_frame(delta_seconds);
		if (_exit_after_frame != 0 && _frame_counter >= _exit_after_frame)
			break ;
	}
}

void Application::step_physics(float delta_seconds)
{
	int	steps_taken;

	_physics_accumulator += delta_seconds;
	steps_taken = 0;
	while (_physics_accumulator >= kFixedDt && steps_taken < 5)
	{
		_physics.step(kFixedDt);
		_physics_accumulator -= kFixedDt;
		steps_taken++;
	}
}

void Application::report_fps_if_due(float delta_seconds)
{
	const FrameStats	*stats;

	_fps_report_accumulator += delta_seconds;
	if (_fps_report_accumulator < 1.0f)
		return ;
	stats = &_renderer.get_last_frame_stats();
	std::fprintf(stderr,
		"FPS: %.1f (%.2f ms/frame) | items: %u total, %u frustum-visible, %u drawn\n",
		static_cast<float>(_frames_this_window) / _fps_report_accumulator,
		1000.0f * _fps_report_accumulator / static_cast<float>(_frames_this_window),
		stats->total_items(), stats->frustum_visible(), stats->drawn());
	_frames_this_window = 0;
	_fps_report_accumulator = 0.0f;
}

void Application::update_and_draw_frame(float delta_seconds)
{
	mat4	view;
	mat4	projection;
	float	aspect;
	float	motion_blur_x;
	float	motion_blur_y;

	std::vector<mat4> pendulum_bone_matrices;
	std::vector<RenderItem> items;
	_camera.update(*_window, delta_seconds, _auto_yaw_speed);
	handle_door_interaction(delta_seconds);
	handle_light_switch_interaction();
	if (_window->was_key_pressed(Window::Key::H))
		_scene.toggle_visible("rig");
			// only meaningful when demo_scene.json is loaded
	step_physics(delta_seconds);
	_scene.sync_from_physics(_physics);
	_scene.update(delta_seconds);
	_steam.update(delta_seconds);
	_pendulum_animator.update(delta_seconds);
	_pendulum_animator.compute_bone_matrices(&pendulum_bone_matrices);
	view = _camera.view_matrix();
	aspect = static_cast<float>(_window->get_width())
		/ static_cast<float>(_window->get_height() > 0 ? _window->get_height() : 1);
	projection = mat4::perspective(kVerticalFov, aspect, 0.1f, 100.0f);
	motion_blur_x = _camera.screen_space_velocity_x(kVerticalFov, aspect);
	motion_blur_y = _camera.screen_space_velocity_y(kVerticalFov);
	_scene.collect_render_items(&items);
	if (_is_house_scene)
	{
		_steam.collect_render_items(&items);
		// Left at its default (opted out of occlusion culling — see
		// RenderItem::occlusion_id's doc comment): one small object near
		// the ceiling, not worth the query-pool bookkeeping.
		items.push_back(RenderItem(_pendulum_mesh, _pendulum_model));
	}
	_renderer.draw_frame(view, projection, _camera.position(),
		_scene.get_lights(), _scene.get_ambient(), items, motion_blur_x,
		motion_blur_y,
		_is_house_scene ? pendulum_bone_matrices : std::vector<mat4>{});
	_frame_counter++;
	if (_screenshot_path != nullptr
		&& _frame_counter == _screenshot_after_frame)
		_renderer.capture_screenshot(_screenshot_path);
	_frames_this_window++;
	report_fps_if_due(delta_seconds);
}

} // namespace vre
