/**
 * @file visibilitycomponent.hpp
 * @brief An entity's own visibility flag (combined with its ancestors' —
 * see Scene::collect_render_items()).
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class VisibilityComponent
{
  public:
	VisibilityComponent();
	VisibilityComponent(const VisibilityComponent &other);
	VisibilityComponent &operator=(const VisibilityComponent &other);
	~VisibilityComponent();

	explicit VisibilityComponent(bool visible);

	/** Editable at runtime; hides this entity AND its subtree. */
	bool visible() const;
	void set_visible(bool value);

  private:
	bool _visible;
};

} // namespace vre
