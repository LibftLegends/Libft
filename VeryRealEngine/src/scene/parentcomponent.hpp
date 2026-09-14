/**
 * @file parentcomponent.hpp
 * @brief Marks an entity as a child of another entity, for hierarchical
 * transforms/visibility.
 */
#pragma once

#include "../ecs/entity.hpp"
#include "../vre.hpp"

namespace vre
{
class ParentComponent
{
  public:
	ParentComponent();
	ParentComponent(const ParentComponent &other);
	ParentComponent &operator=(const ParentComponent &other);
	~ParentComponent();

	explicit ParentComponent(ecs::Entity parent);

	/** The parent entity. */
	ecs::Entity parent() const;
	void set_parent(ecs::Entity value);

  private:
	ecs::Entity _parent;
};

} // namespace vre
