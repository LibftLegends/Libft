/**
 * @file contactresolver.hpp
 * @brief Impulse-based collision response (restitution + Coulomb friction)
 * plus positional correction, applied to two colliding PhysicsBody objects.
 */
#pragma once

#include "../vre.hpp"
#include "contact.hpp"
#include "physicsbody.hpp"

namespace vre
{
class ContactResolver
{
  public:
	ContactResolver();
	ContactResolver(const ContactResolver &other);
	ContactResolver &operator=(const ContactResolver &other);
	~ContactResolver();

	/// Applies impulse-based collision response (restitution + friction)
	/// between `a` and `b`.
	static void resolve(PhysicsBody *a, PhysicsBody *b,
		const Contact &contact);
};

} // namespace vre
