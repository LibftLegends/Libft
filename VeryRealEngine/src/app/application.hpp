/**
 * @file application.hpp
 * @brief Owns the whole demo: window, renderer, physics, scene, particle
 * steam emitter, pendulum skeletal animation, audio, and the first-person
 * camera — and drives the main loop. See main.cpp for the 2-line shim
 * that constructs one and calls run().
 */
#pragma once

#include "../animation/animation_clip.hpp"
#include "../animation/animator.hpp"
#include "../animation/skeleton.hpp"
#include "../audio/audio_system.hpp"
#include "../particles/particle_system.hpp"
#include "../physics/physics_world.hpp"
#include "../platform/window.hpp"
#include "../renderer/renderer.hpp"
#include "../scene/scene.hpp"
#include "../vre.hpp"
#include "first_person_camera.hpp"

namespace vre
{

class Application
{
  public:
	static constexpr float kRoomBLightOffIntensity = 0.05f;
	static constexpr float kRoomBLightOnIntensity = 4.0f;
	static constexpr size_t kRoomBLightIndex = 1;
	static constexpr float kInteractRadius = 1.6f;
	static constexpr float kFixedDt = 1.0f / 60.0f;
	static constexpr float kVerticalFov = 0.9f; // ~51 degrees, must match perspective() call

	Application();
	~Application();

	/**
		* @brief Brings up the window/renderer/physics/scene/audio, then runs
		* the main loop until the window closes, Escape is pressed, or
		* VRE_EXIT_AFTER_FRAME is reached.
		* @param scene_path Scene JSON to load (argv[1],
			or house_scene.json by default).
		* @return 0 on a clean run, 1 if any setup step failed.
		*/
	int run(const char *scene_path);

  private:
	// Owns a Renderer/Scene (both already non-copyable, holding Vulkan/ecs
	// resources) — the pre-C++11 idiom of a private, never-defined copy
	// constructor/assignment operator (this project avoids `= delete`).
	Application(const Application &other);
	Application &operator=(const Application &other);

	/// @return false if window/renderer/scene setup failed.
	bool initialize(const char *scene_path);
	void load_pendulum_and_steam();
	void initialize_audio();
	/// Reads the VRE_* debug/verification environment overrides (see main.cpp's former doc comment).
	void apply_environment_overrides();
	void shutdown();

	void main_loop();
	/// One frame: input, physics step, scene/particle/animation update, draw.
	void update_and_draw_frame(float delta_seconds);
	void handle_door_interaction(float delta_seconds);
	void handle_light_switch_interaction();
	void step_physics(float delta_seconds);
	void report_fps_if_due(float delta_seconds);

	Window *_window;
	Renderer _renderer;
	PhysicsWorld _physics;
	Scene _scene;
	bool _is_house_scene;

	ParticleSystem _steam;
	Skeleton _pendulum_skeleton;
	AnimationClip _pendulum_clip;
	Animator _pendulum_animator;
	MeshHandle _pendulum_mesh;
	mat4 _pendulum_model;

	AudioSystem *_audio;
	bool _audio_available;
	SoundHandle _ambient_sound;
	SoundHandle _click_sound;
	SoundHandle _door_creak_sound;

	FirstPersonCamera _camera;

	bool _door_open;
	float _door_angle;
	float _door_open_angle;
	bool _room_b_light_on;
	float _auto_yaw_speed;

	const char *_screenshot_path;
	uint64_t _screenshot_after_frame;
	uint64_t _frame_counter;
	uint64_t _exit_after_frame;

	float _physics_accumulator;
	uint32_t _frames_this_window;
	float _fps_report_accumulator;
};

} // namespace vre
