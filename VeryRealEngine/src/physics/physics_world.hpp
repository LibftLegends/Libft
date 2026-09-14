/**
 * @file physics_world.hpp
 * @brief Fixed-timestep physics world: gravity, box/sphere collision
 * (broad-phase AABB pretest + exact narrow-phase), impulse-based response
 * with restitution and Coulomb friction, and trigger volumes.
 */
#pragma once

#include "../vre.hpp"
#include "body_handle.hpp"
#include "rigid_body_desc.hpp"

namespace vre
{

class PhysicsWorld
{
  public:
	/**
		* @brief Fired when two trigger-involving bodies start or stop overlapping.
		*
		* Edge-triggered — called once per transition, not every step they
		* overlap. `entered` is true on the frame overlap begins,
			false when it ends.
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

	/** Sets the world's gravity acceleration (applied to every body, scaled by its gravity_scale). */
	void set_gravity(const vec3 &gravity);
	/// Sets the callback invoked on trigger enter/exit transitions.
	void set_trigger_callback(TriggerCallback callback);

	/**
		* @brief Creates a rigid body from `desc`.
		* @return A handle to the new body.
		*/
	BodyHandle add_body(const RigidBodyDesc &desc);

	/// @return The current world-space position of the body identified by `handle`.
	const vec3 &get_position(BodyHandle handle) const;
	/// @return The current linear velocity of the body identified by `handle`.
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
	/// Pure implementation detail — an axis-aligned box used only for the broad-phase pretest.
	class Aabb
	{
		public:
		Aabb();
		Aabb(const Aabb &other);
		Aabb &operator=(const Aabb &other);
		~Aabb();

		vec3 min_corner;
		vec3 max_corner;
	};

	/// The result of a narrow-phase collision test between two bodies.
	class Contact
	{
		public:
		Contact();
		Contact(const Contact &other);
		Contact &operator=(const Contact &other);
		~Contact();

		bool valid;
		vec3 normal; ///< Points from body A toward body B.
		float penetration;
	};

	/// Internal per-body simulation state.
	class Body
	{
		public:
		Body();
		Body(const Body &other);
		Body &operator=(const Body &other);
		~Body();

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
	/** Currently-overlapping trigger pairs (for edge detection). */
	std::set<std::pair<size_t, size_t>> _active_trigger_pairs;

	/// @return The world-space AABB currently enclosing `body`.
	static Aabb compute_aabb(const Body &body);
	/// @return true if AABBs `a` and `b` overlap.
	static bool aabb_overlap(const Aabb &a, const Aabb &b);
	/// @return The narrow-phase contact between `a` and `b` (Contact::valid is false if they don't touch).
	static Contact collide(const Body &a, const Body &b);
	static Contact collide_box_box(const vec3 &pos_a, const vec3 &half_a,
		const vec3 &pos_b, const vec3 &half_b);
	static Contact collide_sphere_sphere(const vec3 &pos_a, float radius_a,
		const vec3 &pos_b, float radius_b);
	/// Contact normal points from the box toward the sphere.
	static Contact collide_box_sphere(const vec3 &box_pos,
		const vec3 &half_extents, const vec3 &sphere_pos, float radius);

	/// Integrates gravity and velocity into position for every body over `dt`.
	void integrate(float dt);
	/** Runs broad + narrow-phase collision detection and dispatches resolution/trigger events. */
	void detect_and_resolve();
	/** Applies impulse-based collision response (restitution + friction) between `a` and `b`. */
	void resolve_contact(Body &a, Body &b, const Contact &contact);
};

} // namespace vre
