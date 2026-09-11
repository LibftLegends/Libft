// VeryRealEngine — step 6/7 milestone: the Interactive House Environment
// (see ../verdict.md roadmap and the subject's Chapter V). Two rooms with
// different lighting (Room A bright, Room B starts dark), a door that
// swings open on a hinge (E to interact near it), a light switch (F to
// interact near it) that turns Room B's light on/off, a steam-emitting
// coffee machine (this engine's particle system), and first-person
// keyboard navigation (WASD to move, arrow keys to look).
//
// assets/scenes/demo_scene.json (the step 1-5 tech demo: physics, PBR
// materials, multi-light shadows) is still in the repo and still loads
// fine — pass its path as argv[1] to see it instead of the house.
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "scene/scene.hpp"
#include "physics/physics_world.hpp"
#include "particles/particle_system.hpp"
#include "math/vre_math.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

// Clamps pitch so the player can't flip the camera past straight up/down.
float clamp_pitch(float pitch)
{
    const float limit = 1.4f; // radians, just under 90 degrees
    if (pitch > limit) return limit;
    if (pitch < -limit) return -limit;
    return pitch;
}

} // namespace

int main(int argc, char **argv)
{
    const char *scene_path = (argc > 1) ? argv[1] : "assets/scenes/house_scene.json";

    vre::Window *window = vre::Window::create();
    if (!window->initialize("VeryRealEngine", 1280, 720))
    {
        delete window;
        return 1;
    }

    vre::Renderer renderer;
    if (!renderer.initialize(window))
    {
        window->destroy();
        delete window;
        return 1;
    }

    vre::PhysicsWorld physics;
    physics.set_trigger_callback([](const std::string &a, const std::string &b, bool entered)
    {
        std::fprintf(stderr, "Trigger: %s %s %s\n", a.c_str(),
            entered ? "entered by" : "exited by", b.c_str());
    });

    vre::Scene scene;
    if (!scene.load(scene_path, &renderer, &physics))
    {
        renderer.destroy();
        window->destroy();
        delete window;
        return 1;
    }

    // Steam particle emitter above the coffee machine (assets/scenes/house_scene.json
    // places it at [-4.5, 0.4, 2.2] with height 0.8, so its top is at y=0.8).
    vre::ParticleSystemDesc steam_desc;
    steam_desc.emitter_position = vre::vec3(-4.5f, 0.85f, 2.2f);
    steam_desc.mesh = renderer.load_mesh_from_obj("assets/models/cube_steam.obj");
    vre::ParticleSystem steam(steam_desc);

    // First-person player state. Room A (bright) is centered near x=-3;
    // start facing toward the doorway at x=-1.
    vre::vec3 player_position(-4.0f, 1.6f, 0.5f);
    float yaw = 1.3f;    // facing roughly toward the doorway/Room B
    float pitch = -0.08f; // a touch downward, a slightly more natural eye-line

    // Door and light-switch interaction state.
    bool door_open = false;
    float door_angle = 0.0f;
    const float door_open_angle = 1.35f; // radians, ~77 degrees
    bool room_b_light_on = false;
    const float room_b_light_off_intensity = 0.05f;
    const float room_b_light_on_intensity = 4.0f;
    const size_t room_b_light_index = 1;
    const float interact_radius = 1.6f;

    // Debug/verification overrides, read from the environment rather than
    // argv: this is purely for scripted screenshot capture (see
    // VRE_SCREENSHOT_PATH below) in a context with no keyboard/mouse to
    // actually drive the demo interactively — e.g. positioning the camera
    // to look at a specific object, or forcing the door/light into a
    // particular state without waiting for real input. None of this
    // affects normal interactive use: every env var is optional and only
    // read once, at startup.
    if (const char *env = std::getenv("VRE_CAMERA_POS"))
    {
        float x, y, z;
        if (std::sscanf(env, "%f,%f,%f", &x, &y, &z) == 3)
            player_position = vre::vec3(x, y, z);
    }
    if (const char *env = std::getenv("VRE_CAMERA_YAW"))
        yaw = static_cast<float>(std::atof(env));
    if (const char *env = std::getenv("VRE_CAMERA_PITCH"))
        pitch = static_cast<float>(std::atof(env));
    if (std::getenv("VRE_FORCE_DOOR_OPEN") != nullptr)
    {
        door_open = true;
        door_angle = door_open_angle; // skip the lerp-open animation entirely
    }
    if (std::getenv("VRE_FORCE_LIGHT_ON") != nullptr)
        room_b_light_on = true;
    // Exercises the exact mechanism Chapter IV.1's "hide an object and all
    // its children simultaneously by editing only the parent" is evaluated
    // against (Scene::set_visible), without needing a live H-keypress —
    // see collect_render_items()'s parent-chain visibility combination.
    if (const char *env = std::getenv("VRE_FORCE_HIDE_NODE"))
        scene.set_visible(env, false);
    // Continuously pans the camera at a fixed angular speed (radians/sec) —
    // exists purely so the post-process motion-blur bonus effect (which
    // needs the camera to actually be turning between frames) can be
    // exercised and screenshotted without live keyboard input.
    float auto_yaw_speed = 0.0f;
    if (const char *env = std::getenv("VRE_AUTO_YAW_SPEED"))
        auto_yaw_speed = static_cast<float>(std::atof(env));

    // Enforce the initial state explicitly rather than relying on it
    // happening to match whatever intensity house_scene.json authored for
    // this light — otherwise the two could silently drift out of sync.
    scene.set_light_intensity(room_b_light_index,
        room_b_light_on ? room_b_light_on_intensity : room_b_light_off_intensity);

    std::fprintf(stderr,
        "Controls: WASD move, arrow keys look, E near the door to open/close it, "
        "F near the light switch to toggle Room B's light, H toggles a demo node "
        "(only present in demo_scene.json).\n");

    // Debug screenshot capture: if VRE_SCREENSHOT_PATH is set, dumps the
    // presented frame to a PPM file once frame_counter reaches
    // VRE_SCREENSHOT_FRAME (default 60 — enough frames for the door lerp
    // and physics to settle into whatever this run's overrides above put
    // them into). See Renderer::capture_screenshot().
    const char *screenshot_path = std::getenv("VRE_SCREENSHOT_PATH");
    uint64_t screenshot_after_frame = 60;
    if (const char *env = std::getenv("VRE_SCREENSHOT_FRAME"))
        screenshot_after_frame = static_cast<uint64_t>(std::atoll(env));
    uint64_t frame_counter = 0;

    // Lets the demo terminate itself cleanly after a fixed frame count —
    // exactly the same shutdown path should->close()/Escape would take
    // (renderer.destroy(), window->destroy(), delete window, a normal
    // `return 0`), just triggered without needing keyboard input. Exists
    // so automated tools that need a real, clean process exit (e.g. macOS's
    // `leaks --atExit`, or a scripted smoke test) can run this demo
    // unattended instead of having to kill -9 it.
    uint64_t exit_after_frame = 0;
    if (const char *env = std::getenv("VRE_EXIT_AFTER_FRAME"))
        exit_after_frame = static_cast<uint64_t>(std::atoll(env));

    const float move_speed = 2.5f;   // units/second
    const float look_speed = 2.0f;   // radians/second

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;

    // FPS measurement (Chapter IV.1's "must achieve at least 60 FPS in
    // Release" requirement): a real, running average printed once a
    // second, not a one-off number — reports frame *count* over the
    // window, not 1/delta_seconds of a single frame, which would be far
    // too noisy frame-to-frame to mean anything.
    uint32_t frames_this_window = 0;
    float fps_report_accumulator = 0.0f;

    while (!window->should_close())
    {
        window->poll_events();
        if (window->was_key_pressed(vre::KeyCode::Escape))
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float delta_seconds = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        // Captured before this frame's look input is applied, so the delta
        // below reflects exactly how far the camera turned this frame —
        // main.cpp's approximation of the post-process motion blur's
        // screen-space velocity (see post.frag's header comment on
        // sample_motion_blur() for why this is camera-pan-only, not a full
        // per-object velocity buffer).
        float yaw_before_look = yaw;
        float pitch_before_look = pitch;

        // --- Look (arrow keys) ---------------------------------------------
        if (window->is_key_held(vre::KeyCode::Left))
            yaw -= look_speed * delta_seconds;
        if (window->is_key_held(vre::KeyCode::Right))
            yaw += look_speed * delta_seconds;
        if (window->is_key_held(vre::KeyCode::Up))
            pitch = clamp_pitch(pitch + look_speed * delta_seconds);
        if (window->is_key_held(vre::KeyCode::Down))
            pitch = clamp_pitch(pitch - look_speed * delta_seconds);
        yaw += auto_yaw_speed * delta_seconds;

        // Movement uses a pitch-free forward vector so looking up/down
        // doesn't make the player fly or sink into the floor.
        vre::vec3 flat_forward(std::sin(yaw), 0.0f, -std::cos(yaw));
        vre::vec3 flat_right(std::cos(yaw), 0.0f, std::sin(yaw));

        if (window->is_key_held(vre::KeyCode::W))
            player_position = player_position + flat_forward * (move_speed * delta_seconds);
        if (window->is_key_held(vre::KeyCode::S))
            player_position = player_position - flat_forward * (move_speed * delta_seconds);
        if (window->is_key_held(vre::KeyCode::D))
            player_position = player_position + flat_right * (move_speed * delta_seconds);
        if (window->is_key_held(vre::KeyCode::A))
            player_position = player_position - flat_right * (move_speed * delta_seconds);

        // --- Door interaction (E) -------------------------------------------
        vre::vec3 door_hinge_position;
        if (scene.get_node_position("door_hinge", &door_hinge_position))
        {
            vre::vec3 to_door = door_hinge_position - player_position;
            float distance = std::sqrt(vre::vec3::dot(to_door, to_door));
            if (distance <= interact_radius && window->was_key_pressed(vre::KeyCode::E))
            {
                door_open = !door_open;
                std::fprintf(stderr, "Door %s.\n", door_open ? "opened" : "closed");
            }
        }
        float door_target_angle = door_open ? door_open_angle : 0.0f;
        door_angle += (door_target_angle - door_angle) * std::min(1.0f, delta_seconds * 5.0f);
        scene.set_node_rotation("door_hinge", vre::vec3(0.0f, door_angle, 0.0f));

        // --- Light switch interaction (F) ------------------------------------
        vre::vec3 switch_position;
        if (scene.get_node_position("light_switch", &switch_position))
        {
            vre::vec3 to_switch = switch_position - player_position;
            float distance = std::sqrt(vre::vec3::dot(to_switch, to_switch));
            if (distance <= interact_radius && window->was_key_pressed(vre::KeyCode::F))
            {
                room_b_light_on = !room_b_light_on;
                scene.set_light_intensity(room_b_light_index,
                    room_b_light_on ? room_b_light_on_intensity : room_b_light_off_intensity);
                std::fprintf(stderr, "Room B light %s.\n", room_b_light_on ? "on" : "off");
            }
        }

        if (window->was_key_pressed(vre::KeyCode::H))
            scene.toggle_visible("rig"); // only meaningful when demo_scene.json is loaded

        // --- Physics / scene / particle update -------------------------------
        const float fixed_dt = 1.0f / 60.0f;
        static float physics_accumulator = 0.0f;
        physics_accumulator += delta_seconds;
        int steps_taken = 0;
        while (physics_accumulator >= fixed_dt && steps_taken < 5)
        {
            physics.step(fixed_dt);
            physics_accumulator -= fixed_dt;
            steps_taken++;
        }
        scene.sync_from_physics(physics);
        scene.update(delta_seconds);
        steam.update(delta_seconds);

        // --- Camera -----------------------------------------------------------
        vre::vec3 look_forward(
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            -std::cos(yaw) * std::cos(pitch));
        vre::mat4 view = vre::mat4::look_at(player_position, player_position + look_forward,
            vre::vec3(0.0f, 1.0f, 0.0f));

        float aspect = static_cast<float>(window->get_width())
            / static_cast<float>(window->get_height() > 0 ? window->get_height() : 1);
        const float vertical_fov = 0.9f; // ~51 degrees, must match perspective() below
        vre::mat4 projection = vre::mat4::perspective(vertical_fov, aspect, 0.1f, 100.0f);

        // Motion-blur velocity: this frame's yaw/pitch change as a fraction
        // of the camera's horizontal/vertical field of view — e.g. turning
        // by exactly the full horizontal FOV in one frame would sweep the
        // whole screen width, hence dividing by it. A deliberately simple
        // camera-only approximation (see post.frag's header comment).
        float horizontal_fov = 2.0f * std::atan(std::tan(vertical_fov * 0.5f) * aspect);
        float motion_blur_x = (yaw - yaw_before_look) / horizontal_fov;
        float motion_blur_y = (pitch - pitch_before_look) / vertical_fov;

        std::vector<vre::RenderItem> items;
        scene.collect_render_items(&items);
        steam.collect_render_items(&items);

        renderer.draw_frame(view, projection, player_position, scene.get_lights(),
            scene.get_ambient(), items, motion_blur_x, motion_blur_y);

        frame_counter++;
        if (screenshot_path != nullptr && frame_counter == screenshot_after_frame)
            renderer.capture_screenshot(screenshot_path);

        frames_this_window++;
        fps_report_accumulator += delta_seconds;
        if (fps_report_accumulator >= 1.0f)
        {
            const vre::Renderer::FrameStats &stats = renderer.get_last_frame_stats();
            std::fprintf(stderr,
                "FPS: %.1f (%.2f ms/frame) | items: %u total, %u frustum-visible, %u drawn\n",
                static_cast<float>(frames_this_window) / fps_report_accumulator,
                1000.0f * fps_report_accumulator / static_cast<float>(frames_this_window),
                stats.total_items, stats.frustum_visible, stats.drawn);
            frames_this_window = 0;
            fps_report_accumulator = 0.0f;
        }

        if (exit_after_frame != 0 && frame_counter >= exit_after_frame)
            break;
    }

    renderer.wait_idle();
    renderer.destroy();
    window->destroy();
    delete window;

    return 0;
}
