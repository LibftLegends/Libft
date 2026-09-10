#include "physics_world.hpp"

#include <algorithm>
#include <cmath>

namespace vre
{

PhysicsWorld::PhysicsWorld() : _gravity(0.0f, -9.81f, 0.0f)
{
}

BodyHandle PhysicsWorld::add_body(const RigidBodyDesc &desc)
{
    Body body;
    body.position = desc.position;
    body.velocity = desc.velocity;
    body.inverse_mass = (desc.is_static || desc.mass <= 0.0f) ? 0.0f : (1.0f / desc.mass);
    body.restitution = desc.restitution;
    body.friction = desc.friction;
    body.gravity_scale = desc.gravity_scale;
    body.is_static = desc.is_static;
    body.is_trigger = desc.is_trigger;
    body.collider = desc.collider;
    body.debug_name = desc.debug_name;

    BodyHandle handle = _bodies.size();
    _bodies.push_back(body);
    return handle;
}

void PhysicsWorld::integrate(float dt)
{
    for (auto &body : _bodies)
    {
        if (body.inverse_mass == 0.0f)
            continue; // static: never moved by the simulation

        // Semi-implicit (symplectic) Euler: update velocity first, then use
        // the new velocity to update position. More stable than explicit
        // Euler for the stiff spring-like forces collision response applies.
        body.velocity = body.velocity + _gravity * (body.gravity_scale * dt);
        body.position = body.position + body.velocity * dt;
    }
}

PhysicsWorld::Aabb PhysicsWorld::compute_aabb(const Body &body)
{
    Aabb box;
    if (body.collider.type == ColliderType::Box)
    {
        box.min_corner = body.position - body.collider.half_extents;
        box.max_corner = body.position + body.collider.half_extents;
    }
    else
    {
        vec3 r(body.collider.radius, body.collider.radius, body.collider.radius);
        box.min_corner = body.position - r;
        box.max_corner = body.position + r;
    }
    return box;
}

bool PhysicsWorld::aabb_overlap(const Aabb &a, const Aabb &b)
{
    return (a.min_corner.x <= b.max_corner.x && a.max_corner.x >= b.min_corner.x)
        && (a.min_corner.y <= b.max_corner.y && a.max_corner.y >= b.min_corner.y)
        && (a.min_corner.z <= b.max_corner.z && a.max_corner.z >= b.min_corner.z);
}

static PhysicsWorld::Contact collide_box_box(const vec3 &pos_a, const vec3 &half_a,
    const vec3 &pos_b, const vec3 &half_b)
{
    PhysicsWorld::Contact contact;
    vec3 delta = pos_b - pos_a;

    float overlap_x = (half_a.x + half_b.x) - std::fabs(delta.x);
    if (overlap_x <= 0.0f)
        return contact;
    float overlap_y = (half_a.y + half_b.y) - std::fabs(delta.y);
    if (overlap_y <= 0.0f)
        return contact;
    float overlap_z = (half_a.z + half_b.z) - std::fabs(delta.z);
    if (overlap_z <= 0.0f)
        return contact;

    // Minimum-translation axis: the axis with the smallest overlap is the
    // one separating the boxes with the least motion — standard AABB SAT.
    contact.valid = true;
    if (overlap_x <= overlap_y && overlap_x <= overlap_z)
    {
        contact.penetration = overlap_x;
        contact.normal = vec3(delta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
    }
    else if (overlap_y <= overlap_z)
    {
        contact.penetration = overlap_y;
        contact.normal = vec3(0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
    }
    else
    {
        contact.penetration = overlap_z;
        contact.normal = vec3(0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f);
    }
    return contact;
}

static PhysicsWorld::Contact collide_sphere_sphere(const vec3 &pos_a, float radius_a,
    const vec3 &pos_b, float radius_b)
{
    PhysicsWorld::Contact contact;
    vec3 delta = pos_b - pos_a;
    float distance = std::sqrt(vec3::dot(delta, delta));
    float penetration = (radius_a + radius_b) - distance;
    if (penetration <= 0.0f)
        return contact;

    contact.valid = true;
    contact.penetration = penetration;
    contact.normal = (distance > 1e-6f) ? (delta * (1.0f / distance)) : vec3(0.0f, 1.0f, 0.0f);
    return contact;
}

// Contact normal points from the box toward the sphere.
static PhysicsWorld::Contact collide_box_sphere(const vec3 &box_pos, const vec3 &half_extents,
    const vec3 &sphere_pos, float radius)
{
    PhysicsWorld::Contact contact;
    vec3 local = sphere_pos - box_pos;
    vec3 closest(
        std::clamp(local.x, -half_extents.x, half_extents.x),
        std::clamp(local.y, -half_extents.y, half_extents.y),
        std::clamp(local.z, -half_extents.z, half_extents.z));

    vec3 delta = local - closest;
    float distance = std::sqrt(vec3::dot(delta, delta));

    if (distance > 1e-6f)
    {
        float penetration = radius - distance;
        if (penetration <= 0.0f)
            return contact;
        contact.valid = true;
        contact.penetration = penetration;
        contact.normal = delta * (1.0f / distance);
    }
    else
    {
        // Sphere center is inside the box: push out along the axis with
        // the least penetration rather than leaving the contact undefined.
        float px = half_extents.x - std::fabs(local.x);
        float py = half_extents.y - std::fabs(local.y);
        float pz = half_extents.z - std::fabs(local.z);
        contact.valid = true;
        if (px <= py && px <= pz)
        {
            contact.penetration = px + radius;
            contact.normal = vec3(local.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
        }
        else if (py <= pz)
        {
            contact.penetration = py + radius;
            contact.normal = vec3(0.0f, local.y < 0.0f ? -1.0f : 1.0f, 0.0f);
        }
        else
        {
            contact.penetration = pz + radius;
            contact.normal = vec3(0.0f, 0.0f, local.z < 0.0f ? -1.0f : 1.0f);
        }
    }
    return contact;
}

PhysicsWorld::Contact PhysicsWorld::collide(const Body &a, const Body &b)
{
    if (a.collider.type == ColliderType::Box && b.collider.type == ColliderType::Box)
        return collide_box_box(a.position, a.collider.half_extents, b.position, b.collider.half_extents);

    if (a.collider.type == ColliderType::Sphere && b.collider.type == ColliderType::Sphere)
        return collide_sphere_sphere(a.position, a.collider.radius, b.position, b.collider.radius);

    if (a.collider.type == ColliderType::Box && b.collider.type == ColliderType::Sphere)
        return collide_box_sphere(a.position, a.collider.half_extents, b.position, b.collider.radius);

    // Sphere vs box: reuse box-vs-sphere and flip the normal, since it's
    // defined to point from the box toward the sphere either way.
    Contact contact = collide_box_sphere(b.position, b.collider.half_extents, a.position, a.collider.radius);
    contact.normal = contact.normal * -1.0f;
    return contact;
}

void PhysicsWorld::resolve_contact(Body &a, Body &b, const Contact &contact)
{
    float total_inverse_mass = a.inverse_mass + b.inverse_mass;
    if (total_inverse_mass <= 0.0f)
        return; // both static (or both zero-mass) — nothing to push apart

    // Positional correction: pull the bodies apart along the contact normal
    // proportional to their share of inverse mass, leaving a small "slop"
    // so resting contacts don't jitter as correction and gravity fight.
    const float correction_percent = 0.2f;
    const float slop = 0.01f;
    float correction_magnitude = std::max(contact.penetration - slop, 0.0f)
        / total_inverse_mass * correction_percent;
    vec3 correction = contact.normal * correction_magnitude;
    a.position = a.position - correction * a.inverse_mass;
    b.position = b.position + correction * b.inverse_mass;

    // Impulse-based velocity response along the normal (restitution).
    vec3 relative_velocity = b.velocity - a.velocity;
    float velocity_along_normal = vec3::dot(relative_velocity, contact.normal);
    if (velocity_along_normal > 0.0f)
        return; // already separating, no impulse needed

    float restitution = std::min(a.restitution, b.restitution);
    float normal_impulse_magnitude = -(1.0f + restitution) * velocity_along_normal / total_inverse_mass;
    vec3 normal_impulse = contact.normal * normal_impulse_magnitude;
    a.velocity = a.velocity - normal_impulse * a.inverse_mass;
    b.velocity = b.velocity + normal_impulse * b.inverse_mass;

    // Coulomb friction: a tangential impulse opposing relative sliding,
    // capped by mu * normal impulse so friction never accelerates sliding.
    relative_velocity = b.velocity - a.velocity;
    float remaining_normal_component = vec3::dot(relative_velocity, contact.normal);
    vec3 tangent_velocity = relative_velocity - contact.normal * remaining_normal_component;
    float tangent_speed = std::sqrt(vec3::dot(tangent_velocity, tangent_velocity));
    if (tangent_speed > 1e-6f)
    {
        vec3 tangent = tangent_velocity * (1.0f / tangent_speed);
        float tangent_impulse_magnitude = -vec3::dot(relative_velocity, tangent) / total_inverse_mass;
        float mu = std::sqrt(a.friction * b.friction);
        float max_friction_impulse = mu * normal_impulse_magnitude;
        tangent_impulse_magnitude = std::clamp(tangent_impulse_magnitude,
            -max_friction_impulse, max_friction_impulse);

        vec3 friction_impulse = tangent * tangent_impulse_magnitude;
        a.velocity = a.velocity - friction_impulse * a.inverse_mass;
        b.velocity = b.velocity + friction_impulse * b.inverse_mass;
    }
}

void PhysicsWorld::detect_and_resolve()
{
    std::set<std::pair<size_t, size_t>> current_trigger_pairs;

    for (size_t i = 0; i < _bodies.size(); i++)
    {
        for (size_t j = i + 1; j < _bodies.size(); j++)
        {
            Body &a = _bodies[i];
            Body &b = _bodies[j];

            if (a.inverse_mass == 0.0f && b.inverse_mass == 0.0f)
                continue; // two static/kinematic bodies never need resolving

            // Broad phase: cheap AABB pretest before the exact shape test.
            if (!aabb_overlap(compute_aabb(a), compute_aabb(b)))
                continue;

            // Narrow phase: exact box/sphere test with normal + penetration.
            Contact contact = collide(a, b);
            if (!contact.valid)
                continue;

            if (a.is_trigger || b.is_trigger)
            {
                // Triggers detect overlap but never block movement — no
                // positional correction, no velocity change, just an
                // edge-triggered enter/exit notification.
                current_trigger_pairs.insert({i, j});
                continue;
            }

            resolve_contact(a, b, contact);
        }
    }

    if (_trigger_callback)
    {
        for (const auto &pair : current_trigger_pairs)
        {
            if (_active_trigger_pairs.find(pair) == _active_trigger_pairs.end())
                _trigger_callback(_bodies[pair.first].debug_name, _bodies[pair.second].debug_name, true);
        }
        for (const auto &pair : _active_trigger_pairs)
        {
            if (current_trigger_pairs.find(pair) == current_trigger_pairs.end())
                _trigger_callback(_bodies[pair.first].debug_name, _bodies[pair.second].debug_name, false);
        }
    }
    _active_trigger_pairs = current_trigger_pairs;
}

void PhysicsWorld::step(float fixed_dt)
{
    integrate(fixed_dt);
    detect_and_resolve();
}

} // namespace vre
