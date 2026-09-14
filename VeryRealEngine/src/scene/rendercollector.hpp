/**
 * @file rendercollector.hpp
 * @brief The render-collection "system" (in the ECS sense): walks a
 * scene's entities in parent-before-child order, composes each one's
 * world transform, combines visibility down the parent chain, and emits
 * a RenderItem for every visible, mesh-carrying entity. Extracted out of
 * Scene as a genuine collaborator — Scene owns the data, this class owns
 * the per-frame algorithm over it.
 */
#pragma once

#include "../ecs/registry.hpp"
#include "../renderer/renderitem.hpp"
#include "../components/mesh.hpp"
#include "../components/parent.hpp"
#include "transformcomposer.hpp"
#include "../components/visibility.hpp"
#include "../components/worldtransform.hpp"
#include <unordered_map>
#include <vector>

namespace vre
{
class SceneRenderCollector
{
	public:
		SceneRenderCollector();
		~SceneRenderCollector();

		/**
		 * @brief Recomputes WorldTransformComponent for every entity in
		 * `load_order` and appends a RenderItem for each visible,
		 * mesh-carrying one.
		 * @param registry Component storage; WorldTransformComponent is
		 * written back into it as a frame-local cache.
		 * @param load_order Entities in parent-before-child order.
		 * @param out_items Render items for this frame are appended here.
		 */
		void collect(ecs::Registry &registry,
			const std::vector<ecs::Entity> &load_order,
			std::vector<RenderItem> *out_items) const;

	private:
		SceneRenderCollector(const SceneRenderCollector &other);
		SceneRenderCollector &operator=(const SceneRenderCollector &other);

		TransformComposer _composer;
};

} // namespace vre
