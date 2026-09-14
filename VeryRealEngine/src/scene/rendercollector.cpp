#include "rendercollector.hpp"

namespace vre
{
SceneRenderCollector::SceneRenderCollector()
{
}

SceneRenderCollector::~SceneRenderCollector()
{
}

void SceneRenderCollector::collect(ecs::Registry &registry,
	const std::vector<ecs::Entity> &load_order,
	std::vector<RenderItem> *out_items) const
{
	// Requires parents to precede children in load_order (enforced by
	// Scene::load()), so a single forward pass suffices to compose every
	// entity's WorldTransformComponent and combined (self AND every
	// ancestor) visibility — no recursion needed.
	std::unordered_map<ecs::Entity, bool> combined_visible;
	combined_visible.reserve(load_order.size());

	for (ecs::Entity entity : load_order)
	{
		const TransformComponent *transform =
			registry.try_get<TransformComponent>(entity);
		const VisibilityComponent *visibility =
			registry.try_get<VisibilityComponent>(entity);
		bool self_visible = (visibility == nullptr) || visibility->visible();

		mat4 local = (transform != nullptr)
			? _composer.compose_local(*transform) : mat4::identity();
		const ParentComponent *parent =
			registry.try_get<ParentComponent>(entity);

		mat4 world;
		bool visible;
		if (parent == nullptr || parent->parent() == ecs::Entity::invalid())
		{
			world = local;
			visible = self_visible;
		}
		else
		{
			const WorldTransformComponent *parent_world =
				registry.try_get<WorldTransformComponent>(parent->parent());
			world = (parent_world != nullptr)
				? mat4::multiply(parent_world->value(), local) : local;
			visible = self_visible && combined_visible[parent->parent()];
		}

		registry.emplace<WorldTransformComponent>(entity,
			WorldTransformComponent(world));
		combined_visible[entity] = visible;

		const MeshComponent *mesh = registry.try_get<MeshComponent>(entity);
		if (visible && mesh != nullptr && mesh->mesh() != MeshComponent::no_mesh())
		{
			// Entity value doubles as the occlusion-culling identity
			// (RenderItem::occlusion_id) — stable across frames as long
			// as the scene's entity set doesn't change shape, which holds
			// for every JSON-authored scene this engine loads.
			RenderItem item(mesh->mesh(), world);
			item.set_occlusion_id(entity.value());
			out_items->push_back(item);
		}
	}
}

} // namespace vre
