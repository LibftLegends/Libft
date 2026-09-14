#include "application.hpp"

namespace vre
{
Application::Application() : _window(nullptr), _is_house_scene(false),
	_screenshot_path(nullptr), _screenshot_after_frame(60),
	_frame_counter(0), _exit_after_frame(0), _auto_yaw_speed(0.0f),
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

bool Application::initialize(const char *scene_path)
{
	DebugConfig			debug_config;
	ParticleSystemDesc	steam_desc;

	_is_house_scene = std::string(scene_path).find("house_scene")
		!= std::string::npos;
	_window = Window::create();
	if (!_window->initialize("VeryRealEngine", 1280, 720))
	{
		delete _window;
		_window = nullptr;
		return (false);
	}
	if (!_renderer.initialize(_window))
	{
		_window->destroy();
		delete _window;
		_window = nullptr;
		return (false);
	}
	_physics.set_trigger_callback([](const std::string &a,
			const std::string &b, bool entered) { std::fprintf(stderr,
			"Trigger: %s %s %s\n", a.c_str(),
			entered ? "entered by" : "exited by", b.c_str()); });
	if (!_scene.load(scene_path, &_renderer, &_physics))
	{
		_renderer.destroy();
		_window->destroy();
		delete _window;
		_window = nullptr;
		return (false);
	}
	// Steam particle emitter above the coffee machine (house_scene.json
	// places it at [-4.5, 0.4, 2.2] with height 0.8, so its top is at y=0.8).
	steam_desc.set_emitter_position(vec3(-4.5f, 0.85f, 2.2f));
	steam_desc.set_mesh(_renderer.load_mesh_from_obj(
			"assets/models/cube_steam.obj"));
	_steam = ParticleSystem(steam_desc);
	// Skeletal animation (bonus, Chapter VII): a small hanging pendulum
	// lamp — a 3-bone chain, each bone swinging a bit further than its
	// parent for a real, cascading pendulum motion, GPU-skinned via
	// Renderer::draw_frame()'s bone_matrices parameter (see
	// meshdata.hpp's MeshVertex doc comment for how a single shader path
	// serves both this and every static mesh in the scene). Hangs from
	// Room A's ceiling, clear of the light/door/coffee machine.
	_pendulum.load(&_renderer,
		"assets/models/pendulum_lamp.skinnedmesh.json",
		vec3(-2.2f, 2.95f, 1.5f));
	_audio.initialize();
	debug_config.apply_camera(&_camera);
	debug_config.apply_door(&_door);
	debug_config.apply_light_switch(&_light_switch);
	debug_config.apply_scene(&_scene);
	_auto_yaw_speed = debug_config.auto_yaw_speed();
	_screenshot_path = debug_config.screenshot_path();
	_screenshot_after_frame = debug_config.screenshot_after_frame();
	_exit_after_frame = debug_config.exit_after_frame();
	_light_switch.apply_intensity(&_scene);
	std::fprintf(stderr,
		"Controls: WASD move, arrow keys look, E near the door to "
		"open/close it, F near the light switch to toggle Room B's "
		"light, H toggles a demo node (only present in demo_scene.json).\n");
	return (true);
}

void Application::shutdown()
{
	_audio.destroy();
	_renderer.wait_idle();
	_renderer.destroy();
	_window->destroy();
	delete _window;
	_window = nullptr;
}

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
		"FPS: %.1f (%.2f ms/frame) | items: %u total, %u frustum-visible, "
		"%u drawn\n",
		static_cast<float>(_frames_this_window) / _fps_report_accumulator,
		1000.0f * _fps_report_accumulator
			/ static_cast<float>(_frames_this_window),
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
	_door.update(delta_seconds, &_scene, _window, _camera.position(),
		&_audio);
	_light_switch.update(&_scene, _window, _camera.position(), &_audio);
	if (_window->was_key_pressed(Window::Key::H))
		_scene.toggle_visible("rig");
			// only meaningful when demo_scene.json is loaded
	step_physics(delta_seconds);
	_scene.sync_from_physics(_physics);
	_scene.update(delta_seconds);
	_steam.update(delta_seconds);
	_pendulum.update(delta_seconds);
	_pendulum.collect_bone_matrices(&pendulum_bone_matrices);
	view = _camera.view_matrix();
	aspect = static_cast<float>(_window->get_width())
		/ static_cast<float>(_window->get_height() > 0
			? _window->get_height() : 1);
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
		items.push_back(_pendulum.render_item());
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
