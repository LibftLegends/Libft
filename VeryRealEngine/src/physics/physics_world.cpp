#include "physics_world.hpp"

namespace vre
{

PhysicsWorld::PhysicsWorld() : _gravity(0.0f, -9.81f, 0.0f)
{
}

PhysicsWorld::PhysicsWorld(const PhysicsWorld &other) : _bodies(other._bodies),
	_gravity(other._gravity), _trigger_callback(other._trigger_callback),
	_active_trigger_pairs(other._active_trigger_pairs)
{
}

PhysicsWorld &PhysicsWorld::operator=(const PhysicsWorld &other)
{
	if (this != &other)
	{
		_bodies = other._bodies;
		_gravity = other._gravity;
		_trigger_callback = other._trigger_callback;
		_active_trigger_pairs = other._active_trigger_pairs;
	}
	return (*this);
}

PhysicsWorld::~PhysicsWorld()
{
}

void PhysicsWorld::set_gravity(const vec3 &gravity)
{
	_gravity = gravity;
}

void PhysicsWorld::set_trigger_callback(TriggerCallback callback)
{
	_trigger_callback = callback;
}

BodyHandle PhysicsWorld::add_body(const RigidBodyDesc &desc)
{
	Body	body;

	body.position = desc.position();
	body.velocity = desc.velocity();
	body.inverse_mass = (desc.is_static() || desc.mass() <= 0.0f) ? 0.0f : (1.0f
			/ desc.mass());
	body.restitution = desc.restitution();
	body.friction = desc.friction();
	body.gravity_scale = desc.gravity_scale();
	body.is_static = desc.is_static();
	body.is_trigger = desc.is_trigger();
	body.collider = desc.collider();
	body.debug_name = desc.debug_name();
	BodyHandle handle(_bodies.size());
	_bodies.push_back(body);
	return (handle);
}

const vec3 &PhysicsWorld::get_position(BodyHandle handle) const
{
	return (_bodies[handle.value()].position);
}

const vec3 &PhysicsWorld::get_velocity(BodyHandle handle) const
{
	return (_bodies[handle.value()].velocity);
}

void PhysicsWorld::integrate(float dt)
{
	for (auto &body : _bodies)
	{
		if (body.inverse_mass == 0.0f)
			continue ; // static: never moved by the simulation
		// Semi-implicit (symplectic) Euler: update velocity first, then use
		// the new velocity to update position. More stable than explicit
		// Euler for the stiff spring-like forces collision response applies.
		body.velocity = body.velocity + _gravity * (body.gravity_scale * dt);
		body.position = body.position + body.velocity * dt;
	}
}

void PhysicsWorld::detect_and_resolve()
{
	Contact	contact;

	std::set<std::pair<size_t, size_t>> current_trigger_pairs;
	for (size_t i = 0; i < _bodies.size(); i++)
	{
		for (size_t j = i + 1; j < _bodies.size(); j++)
		{
			Body &a = _bodies[i];
			Body &b = _bodies[j];
			if (a.inverse_mass == 0.0f && b.inverse_mass == 0.0f)
				continue ; // two static/kinematic bodies never need resolving
			// Broad phase: cheap AABB pretest before the exact shape test.
			if (!aabb_overlap(compute_aabb(a), compute_aabb(b)))
				continue ;
			// Narrow phase: exact box/sphere test with normal + penetration.
			contact = collide(a, b);
			if (!contact.valid)
				continue ;
			if (a.is_trigger || b.is_trigger)
			{
				// Triggers detect overlap but never block movement — no
				// positional correction, no velocity change, just an
				// edge-triggered enter/exit notification.
				current_trigger_pairs.insert({i, j});
				continue ;
			}
			resolve_contact(a, b, contact);
		}
	}
	if (_trigger_callback)
	{
		for (const auto &pair : current_trigger_pairs)
		{
			if (_active_trigger_pairs.find(pair) == _active_trigger_pairs.end())
				_trigger_callback(_bodies[pair.first].debug_name,
					_bodies[pair.second].debug_name, true);
		}
		for (const auto &pair : _active_trigger_pairs)
		{
			if (current_trigger_pairs.find(pair) == current_trigger_pairs.end())
				_trigger_callback(_bodies[pair.first].debug_name,
					_bodies[pair.second].debug_name, false);
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
