/**
 * @file physicsbody.hpp
 * @brief One rigid body's live simulation state inside a PhysicsWorld:
 * position/velocity, mass/restitution/friction, and its collision shape.
 * See RigidBodyDesc for how one of these is configured at add_body() time.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "collider.hpp"

namespace vre
{
class PhysicsBody
{
  public:
	PhysicsBody();
	PhysicsBody(const PhysicsBody &other);
	PhysicsBody &operator=(const PhysicsBody &other);
	~PhysicsBody();

	vec3 position;
	vec3 velocity;
	/// 0 for a static body — never moved by integration or collision
	/// response.
	float inverse_mass;
	float restitution;
	float friction;
	float gravity_scale;
	bool is_static;
	/// Detects overlap but never blocks movement (see
	/// RigidBodyDesc::is_trigger).
	bool is_trigger;
	Collider collider;
	/// For trigger-callback / log messages only.
	std::string debug_name;
};

} // namespace vre
