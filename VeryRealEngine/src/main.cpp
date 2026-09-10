// VeryRealEngine — step 5 milestone: multiple light sources plus a
// directional-light shadow map (see ../verdict.md roadmap), on top of
// step 4's physics-driven falling cube and step 3's parent-child rig.
//
// assets/scenes/demo_scene.json now also declares "lights": a directional
// sun (the shadow caster) and a point light. Both the still and the moving
// objects (the spinning rig, the falling/settling cube) cast shadows that
// update every frame, since the shadow map is re-rendered every frame.
#include "platform/window.hpp"
#include "renderer/renderer.hpp"
#include "scene/scene.hpp"
#include "physics/physics_world.hpp"
#include "math/vre_math.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

int main()
{
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
    if (!scene.load("assets/scenes/demo_scene.json", &renderer, &physics))
    {
        renderer.destroy();
        window->destroy();
        delete window;
        return 1;
    }

    std::fprintf(stderr, "Press H to toggle the \"rig\" node (and its child) on/off.\n");

    const float fixed_dt = 1.0f / 60.0f;
    const int max_physics_steps_per_frame = 5; // caps the "spiral of death" on long stalls
    float physics_accumulator = 0.0f;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_time = start_time;

    while (!window->should_close())
    {
        window->poll_events();

        auto now = std::chrono::high_resolution_clock::now();
        float delta_seconds = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        if (window->was_key_pressed(vre::KeyCode::H))
            scene.toggle_visible("rig");

        // Fixed-timestep physics, decoupled from the variable render frame
        // time: the simulation always advances in fixed_dt increments, so
        // its behavior doesn't depend on frame rate.
        physics_accumulator += delta_seconds;
        int steps_taken = 0;
        while (physics_accumulator >= fixed_dt && steps_taken < max_physics_steps_per_frame)
        {
            physics.step(fixed_dt);
            physics_accumulator -= fixed_dt;
            steps_taken++;
        }
        scene.sync_from_physics(physics);

        scene.update(delta_seconds); // visual-only spin animation, unrelated to physics

        vre::vec3 eye(4.0f, 3.0f, 5.5f);
        vre::mat4 view = vre::mat4::look_at(eye, vre::vec3(0.0f, 0.5f, 0.0f), vre::vec3(0.0f, 1.0f, 0.0f));

        float aspect = static_cast<float>(window->get_width())
            / static_cast<float>(window->get_height() > 0 ? window->get_height() : 1);
        vre::mat4 projection = vre::mat4::perspective(0.78539816f /* 45 deg */, aspect, 0.1f, 100.0f);

        std::vector<vre::RenderItem> items;
        scene.collect_render_items(&items);

        renderer.draw_frame(view, projection, eye, scene.get_lights(), scene.get_ambient(), items);
    }

    renderer.wait_idle();
    renderer.destroy();
    window->destroy();
    delete window;

    return 0;
}
