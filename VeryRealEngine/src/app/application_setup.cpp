#include "application.hpp"

namespace vre
{

bool Application::initialize(const char *scene_path)
{
	_is_house_scene = std::string(scene_path).find("house_scene") != std::string::npos;
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
	_physics.set_trigger_callback([](const std::string &a, const std::string &b,
			bool entered) { std::fprintf(stderr, "Trigger: %s %s %s\n",
			a.c_str(), entered ? "entered by" : "exited by", b.c_str()); });
	if (!_scene.load(scene_path, &_renderer, &_physics))
	{
		_renderer.destroy();
		_window->destroy();
		delete _window;
		_window = nullptr;
		return (false);
	}
	load_pendulum_and_steam();
	initialize_audio();
	apply_environment_overrides();
	_scene.set_light_intensity(kRoomBLightIndex,
		_room_b_light_on ? kRoomBLightOnIntensity : kRoomBLightOffIntensity);
	std::fprintf(stderr,
		"Controls: WASD move, arrow keys look, E near the door to open/close it, "
		"F near the light switch to toggle Room B's light, H toggles a demo node "
		"(only present in demo_scene.json).\n");
	return (true);
}

void Application::load_pendulum_and_steam()
{
	ParticleSystemDesc	steam_desc;

	// Steam particle emitter above the coffee machine (house_scene.json
	// places it at [-4.5, 0.4, 2.2] with height 0.8, so its top is at y=0.8).
	steam_desc.set_emitter_position(vec3(-4.5f, 0.85f, 2.2f));
	steam_desc.set_mesh(_renderer.load_mesh_from_obj("assets/models/cube_steam.obj"));
	_steam = ParticleSystem(steam_desc);
	// Skeletal animation (bonus, Chapter VII): a small hanging pendulum
	// lamp — a 3-bone chain, each bone swinging a bit further than its
	// parent for a real, cascading pendulum motion, GPU-skinned via
	// Renderer::draw_frame()'s bone_matrices parameter (see
	// mesh_data.hpp's MeshVertex doc comment for how a single shader path
	// serves both this and every static mesh in the scene).
	_pendulum_mesh = _renderer.load_skinned_mesh("assets/models/pendulum_lamp.skinnedmesh.json",
			&_pendulum_skeleton, &_pendulum_clip);
	_pendulum_animator = Animator(&_pendulum_skeleton, &_pendulum_clip);
	// Hangs from Room A's ceiling, clear of the light/door/coffee machine.
	_pendulum_model = mat4::translate(vec3(-2.2f, 2.95f, 1.5f));
}

void Application::initialize_audio()
{
	// Sound system (bonus, Chapter VII): a looping ambient room hum plus
	// one-shot effects on the door and light switch. initialize()
	// returning false (no device, e.g. a headless/sandboxed session) is
	// not fatal — see audio_system.hpp's doc comment — the demo just runs
	// silently rather than aborting.
	_audio = AudioSystem::create();
	_audio_available = _audio->initialize();
	if (_audio_available)
	{
		_ambient_sound = _audio->load_sound("assets/sounds/ambient_hum.wav");
		_click_sound = _audio->load_sound("assets/sounds/click.wav");
		_door_creak_sound = _audio->load_sound("assets/sounds/door_creak.wav");
		if (_ambient_sound.is_valid())
			_audio->play(_ambient_sound, /*loop=*/true, /*volume=*/0.35f);
	}
}

void Application::apply_environment_overrides()
{
		float x;
		float y;
		float z;

	// Debug/verification overrides, read from the environment rather than
	// argv: this is purely for scripted screenshot capture in a context
	// with no keyboard/mouse to actually drive the demo interactively —
	// e.g. positioning the camera to look at a specific object, or
	// forcing the door/light into a particular state without waiting for
	// real input. None of this affects normal interactive use: every env
	// var is optional and only read once, at startup.
	if (const char *env = std::getenv("VRE_CAMERA_POS"))
	{
		if (std::sscanf(env, "%f,%f,%f", &x, &y, &z) == 3)
			_camera.set_position(vec3(x, y, z));
	}
	if (const char *env = std::getenv("VRE_CAMERA_YAW"))
		_camera.set_yaw(static_cast<float>(std::atof(env)));
	if (const char *env = std::getenv("VRE_CAMERA_PITCH"))
		_camera.set_pitch(static_cast<float>(std::atof(env)));
	// Overrides how far VRE_FORCE_DOOR_OPEN below opens the door — lets a
	// screenshot show a small, deliberate gap instead of only the full
	// ~77-degree swing.
	if (const char *env = std::getenv("VRE_DOOR_OPEN_ANGLE"))
		_door_open_angle = static_cast<float>(std::atof(env));
	if (std::getenv("VRE_FORCE_DOOR_OPEN") != nullptr)
	{
		_door_open = true;
		_door_angle = _door_open_angle; // skip the lerp-open animation entirely
	}
	if (std::getenv("VRE_FORCE_LIGHT_ON") != nullptr)
		_room_b_light_on = true;
	// Exercises the exact mechanism Chapter IV.1's "hide an object and all
	// its children simultaneously by editing only the parent" is evaluated
	// against (Scene::set_visible), without needing a live H-keypress.
	if (const char *env = std::getenv("VRE_FORCE_HIDE_NODE"))
		_scene.set_visible(env, false);
	// Continuously pans the camera at a fixed angular speed (radians/sec) —
	// exists purely so the post-process motion-blur bonus effect can be
	// exercised and screenshotted without live keyboard input.
	if (const char *env = std::getenv("VRE_AUTO_YAW_SPEED"))
		_auto_yaw_speed = static_cast<float>(std::atof(env));
	// Debug screenshot capture: if VRE_SCREENSHOT_PATH is set, dumps the
	// presented frame to a PPM file once _frame_counter reaches
	// _screenshot_after_frame (default 60). See Renderer::capture_screenshot().
	_screenshot_path = std::getenv("VRE_SCREENSHOT_PATH");
	if (const char *env = std::getenv("VRE_SCREENSHOT_FRAME"))
		_screenshot_after_frame = static_cast<uint64_t>(std::atoll(env));
	// Lets the demo terminate itself cleanly after a fixed frame count —
	// exactly the same shutdown path Escape would take — so automated
	// tools (e.g. macOS's `leaks --atExit`, a scripted smoke test) can run
	// this demo unattended instead of having to kill -9 it.
	if (const char *env = std::getenv("VRE_EXIT_AFTER_FRAME"))
		_exit_after_frame = static_cast<uint64_t>(std::atoll(env));
}

void Application::shutdown()
{
	_audio->destroy();
	delete _audio;
	_audio = nullptr;
	_renderer.wait_idle();
	_renderer.destroy();
	_window->destroy();
	delete _window;
	_window = nullptr;
}

} // namespace vre
