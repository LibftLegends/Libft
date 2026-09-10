// Fixed-timestep physics world: gravity, box/sphere collision (broad-phase
// AABB pretest + exact narrow-phase), impulse-based response with
// restitution and Coulomb friction, and trigger volumes.
#pragma once

#include "physics_types.hpp"

#include <cstddef>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace vre
{

using BodyHandle = size_t;
constexpr BodyHandle kInvalidBody = static_cast<BodyHandle>(-1);

// Fired when two trigger-involving bodies start or stop overlapping
// (edge-triggered — called once per transition, not every step they
// overlap). `entered` is true on the frame overlap begins, false when it ends.
using TriggerCallback = std::function<void(const std::string &name_a,
    const std::string &name_b, bool entered)>;

class PhysicsWorld
{
    public:
        PhysicsWorld();

        void set_gravity(const vec3 &gravity) { _gravity = gravity; }
        void set_trigger_callback(TriggerCallback callback) { _trigger_callback = callback; }

        BodyHandle add_body(const RigidBodyDesc &desc);

        const vec3 &get_position(BodyHandle handle) const { return _bodies[handle].position; }
        const vec3 &get_velocity(BodyHandle handle) const { return _bodies[handle].velocity; }

        // Advances the simulation by exactly `fixed_dt` seconds. Call this
        // from a fixed-timestep accumulator loop (see main.cpp), not once
        // per render frame with a variable dt — that would make the
        // simulation's behavior depend on frame rate.
        void step(float fixed_dt);

        // Exposed (rather than private) only so the free collision-test
        // functions in physics_world.cpp can return them; not part of the
        // intended public API surface.
        struct Aabb
        {
            vec3 min_corner;
            vec3 max_corner;
        };

        struct Contact
        {
            bool valid = false;
            vec3 normal;      // points from body A toward body B
            float penetration = 0.0f;
        };

    private:
        struct Body
        {
            vec3 position;
            vec3 velocity;
            float inverse_mass;
            float restitution;
            float friction;
            float gravity_scale;
            bool is_static;
            bool is_trigger;
            Collider collider;
            std::string debug_name;
        };

        std::vector<Body> _bodies;
        vec3 _gravity;
        TriggerCallback _trigger_callback;
        std::set<std::pair<size_t, size_t>> _active_trigger_pairs;

        static Aabb compute_aabb(const Body &body);
        static bool aabb_overlap(const Aabb &a, const Aabb &b);
        static Contact collide(const Body &a, const Body &b);

        void integrate(float dt);
        void detect_and_resolve();
        void resolve_contact(Body &a, Body &b, const Contact &contact);
};

} // namespace vre
