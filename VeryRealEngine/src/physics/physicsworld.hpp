/**
 * @file physicsworld.hpp
 * @brief Fixed-timestep physics world: gravity, box/sphere collision
 * (broad-phase AABB pretest + exact narrow-phase, via CollisionDetector),
 * impulse-based response (via ContactResolver), and trigger volumes.
 */
#pragma once

#include "../vre.hpp"
#include "bodyhandle.hpp"
#include "collisiondetector.hpp"
#include "contactresolver.hpp"
#include "physicsbody.hpp"
#include "rigidbodydesc.hpp"

namespace vre
{
class PhysicsWorld
{
  public:
	/**
		* @brief Fired when two trigger-involving bodies start or stop
		* overlapping.
		*
		* Edge-triggered — called once per transition, not every step
		* they overlap. `entered` is true on the frame overlap begins,
		* false when it ends.
		* @param name_a Debug name of the first body.
		* @param name_b Debug name of the second body.
		* @param entered true if overlap just began, false if it just ended.
		*/
	using TriggerCallback = std::function<void(const std::string &name_a,
			const std::string &name_b, bool entered)>;

	PhysicsWorld();
	PhysicsWorld(const PhysicsWorld &other);
	PhysicsWorld &operator=(const PhysicsWorld &other);
	~PhysicsWorld();

	/// Sets the world's gravity acceleration (applied to every body,
	/// scaled by its gravity_scale).
	void set_gravity(const vec3 &gravity);
	/// Sets the callback invoked on trigger enter/exit transitions.
	void set_trigger_callback(TriggerCallback callback);

	/**
		* @brief Creates a rigid body from `desc`.
		* @return A handle to the new body.
		*/
	BodyHandle add_body(const RigidBodyDesc &desc);

	/// @return The current world-space position of the body identified
	/// by `handle`.
	const vec3 &get_position(BodyHandle handle) const;
	/// @return The current linear velocity of the body identified by
	/// `handle`.
	const vec3 &get_velocity(BodyHandle handle) const;

	/**
		* @brief Advances the simulation by exactly `fixed_dt` seconds.
		*
		* Call this from a fixed-timestep accumulator loop (see main.cpp),
		* not once per render frame with a variable dt — that would make
		* the simulation's behavior depend on frame rate.
		*/
	void step(float fixed_dt);

  private:
	/// Integrates gravity and velocity into position for every body over `dt`.
	void integrate(float dt);
	/// Runs broad + narrow-phase collision detection (CollisionDetector)
	/// and dispatches resolution (ContactResolver) / trigger events.
	void detect_and_resolve();

	std::vector<PhysicsBody> _bodies;
	vec3 _gravity;
	TriggerCallback _trigger_callback;
	/// Currently-overlapping trigger pairs (for edge detection).
	std::set<std::pair<size_t, size_t>> _active_trigger_pairs;
};

} // namespace vre
