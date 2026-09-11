/**
 * @file physics_types.hpp
 * @brief Shared types for the physics module (step 4 of the roadmap in
 * ../../verdict.md).
 *
 * Deliberately scoped to linear dynamics on axis-aligned box/sphere
 * colliders — no rotational dynamics, no oriented boxes. That covers what
 * the subject's mandatory part actually asks for (collision
 * detection/response, gravity, friction, trigger volumes) without pulling
 * in a full 6-DOF rigid-body solver.
 */
#pragma once

#include "../math/vre_math.hpp"

#include <string>

namespace vre
{

/// Which shape a Collider represents.
enum class ColliderType
{
    Box,    ///< Axis-aligned, defined by half_extents.
    Sphere, ///< Defined by radius.
};

/// A rigid body's collision shape: either an axis-aligned box or a sphere.
struct Collider
{
    ColliderType type = ColliderType::Box;
    vec3 half_extents{0.5f, 0.5f, 0.5f}; ///< Used when type == Box.
    float radius = 0.5f;                 ///< Used when type == Sphere.
};

/**
 * @brief Input to PhysicsWorld::add_body().
 *
 * A static body has infinite mass (never moved by collision response) but
 * still participates in collision detection — that's how the ground plane
 * works here.
 */
struct RigidBodyDesc
{
    vec3 position;
    vec3 velocity;
    float mass = 1.0f;
    float restitution = 0.3f; ///< 0 = fully inelastic, 1 = fully elastic.
    float friction = 0.5f;
    float gravity_scale = 1.0f;
    bool is_static = false;
    bool is_trigger = false; ///< Detects overlap but never blocks movement.
    Collider collider;
    std::string debug_name; ///< For trigger-callback / log messages only.
};

} // namespace vre
