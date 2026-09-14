/**
 * @file worldtransform.hpp
 * @brief Cached world-space transform.
 *
 * Populated every frame by Scene::collect_render_items()'s transform pass,
 * for anything that then needs "where is this entity in the world" without
 * re-walking the parent chain itself (Scene::get_node_position() reuses it).
 */
#pragma once

#include "../math/mat4.hpp"
#include "../vre.hpp"

namespace vre
{
class WorldTransformComponent
{
  public:
	WorldTransformComponent();
	WorldTransformComponent(const WorldTransformComponent &other);
	WorldTransformComponent &operator=(const WorldTransformComponent &other);
	~WorldTransformComponent();

	explicit WorldTransformComponent(const mat4 &value);

	const mat4 &value() const;
	void set_value(const mat4 &value);

  private:
	mat4 _value;
};

} // namespace vre
