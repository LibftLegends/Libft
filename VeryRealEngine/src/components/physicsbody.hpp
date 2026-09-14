/**
 * @file physicsbody.hpp
 * @brief Marks an entity as physics-driven.
 *
 * Presence of this component is what "has a rigid body" means — entities
 * without physics simply never get one added, rather than carrying a
 * sentinel "no body" value.
 */
#pragma once

#include "../physics/bodyhandle.hpp"
#include "../vre.hpp"

namespace vre
{
class PhysicsBodyComponent
{
  public:
	PhysicsBodyComponent();
	PhysicsBodyComponent(const PhysicsBodyComponent &other);
	PhysicsBodyComponent &operator=(const PhysicsBodyComponent &other);
	~PhysicsBodyComponent();

	explicit PhysicsBodyComponent(BodyHandle body);

	BodyHandle body() const;
	void set_body(BodyHandle value);

  private:
	BodyHandle _body;
};

} // namespace vre
