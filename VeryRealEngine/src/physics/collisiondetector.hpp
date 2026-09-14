/**
 * @file collisiondetector.hpp
 * @brief Broad-phase AABB pretest plus exact narrow-phase box/sphere
 * collision testing, decoupled from PhysicsWorld's own body bookkeeping —
 * takes and returns plain positions/colliders/contacts, not PhysicsBody.
 */
#pragma once

#include "../math/aabb.hpp"
#include "../math/vec3.hpp"
#include "../vre.hpp"
#include "collider.hpp"
#include "contact.hpp"

namespace vre
{
class CollisionDetector
{
  public:
	CollisionDetector();
	CollisionDetector(const CollisionDetector &other);
	CollisionDetector &operator=(const CollisionDetector &other);
	~CollisionDetector();

	/// @return The world-space AABB enclosing `collider` placed at
	/// `position`.
	static AABB compute_aabb(const vec3 &position, const Collider &collider);
	/// @return true if AABBs `a` and `b` overlap.
	static bool aabb_overlap(const AABB &a, const AABB &b);

	/**
		* @brief Exact narrow-phase test between two placed colliders.
		* @return The contact between them (Contact::valid is false if
		* they don't touch).
		*/
	static Contact collide(const vec3 &pos_a, const Collider &collider_a,
		const vec3 &pos_b, const Collider &collider_b);

  private:
	static Contact collide_box_box(const vec3 &pos_a, const vec3 &half_a,
		const vec3 &pos_b, const vec3 &half_b);
	static Contact collide_sphere_sphere(const vec3 &pos_a, float radius_a,
		const vec3 &pos_b, float radius_b);
	/// Contact normal points from the box toward the sphere.
	static Contact collide_box_sphere(const vec3 &box_pos,
		const vec3 &half_extents, const vec3 &sphere_pos, float radius);
};

} // namespace vre
