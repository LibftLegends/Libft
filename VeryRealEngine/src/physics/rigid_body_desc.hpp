/**
 * @file rigid_body_desc.hpp
 * @brief Input to PhysicsWorld::add_body().
 *
 * A static body has infinite mass (never moved by collision response) but
 * still participates in collision detection — that's how the ground plane
 * works here.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "collider.hpp"

namespace vre
{

class RigidBodyDesc
{
  public:
	RigidBodyDesc();
	RigidBodyDesc(const RigidBodyDesc &other);
	RigidBodyDesc &operator=(const RigidBodyDesc &other);
	~RigidBodyDesc();

	const vec3 &position() const;
	void set_position(const vec3 &value);

	const vec3 &velocity() const;
	void set_velocity(const vec3 &value);

	float mass() const;
	void set_mass(float value);

	/** 0 = fully inelastic, 1 = fully elastic. */
	float restitution() const;
	void set_restitution(float value);

	float friction() const;
	void set_friction(float value);

	float gravity_scale() const;
	void set_gravity_scale(float value);

	bool is_static() const;
	void set_static(bool value);

	/** Detects overlap but never blocks movement. */
	bool is_trigger() const;
	void set_trigger(bool value);

	const Collider &collider() const;
	Collider &collider();

	/** For trigger-callback / log messages only. */
	const std::string &debug_name() const;
	void set_debug_name(const std::string &value);

  private:
	vec3 _position;
	vec3 _velocity;
	float _mass;
	float _restitution;
	float _friction;
	float _gravity_scale;
	bool _is_static;
	bool _is_trigger;
	Collider _collider;
	std::string _debug_name;
};

} // namespace vre
