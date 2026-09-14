/**
 * @file scene_queries.cpp
 * @brief Scene's per-frame render collection plus name/visibility/rotation
 * query methods, split out of scene.cpp purely to keep each file under
 * this project's 250-line cap — both files define methods of the same
 * Scene class declared in scene.hpp.
 */
#include "scene.hpp"

namespace vre
{

void Scene::collect_render_items(std::vector<RenderItem> *out_items) const
{
    // The transform + visibility system: requires parents to precede
    // children in _load_order (enforced in load()), so a single forward
    // pass suffices to compose every entity's WorldTransformComponent and
    // combined (self AND every ancestor) visibility — no recursion needed.
    // const_cast is safe/contained here: WorldTransformComponent is a
    // frame-local cache, not scene-authored state, and recomputing it is
    // exactly what a const "give me this frame's render items" call means.
    auto &registry = const_cast<ecs::Registry &>(_registry);
    std::unordered_map<ecs::Entity, bool> combined_visible;
    combined_visible.reserve(_load_order.size());

    for (ecs::Entity entity : _load_order)
    {
        const TransformComponent *transform = registry.try_get<TransformComponent>(entity);
        const VisibilityComponent *visibility = registry.try_get<VisibilityComponent>(entity);
        bool self_visible = (visibility == nullptr) || visibility->visible();

        mat4 local = (transform != nullptr) ? local_transform(*transform) : mat4::identity();
        const ParentComponent *parent = registry.try_get<ParentComponent>(entity);

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
            world = (parent_world != nullptr) ? mat4::multiply(parent_world->value(), local) : local;
            visible = self_visible && combined_visible[parent->parent()];
        }

        registry.emplace<WorldTransformComponent>(entity, WorldTransformComponent(world));
        combined_visible[entity] = visible;

        const MeshComponent *mesh = registry.try_get<MeshComponent>(entity);
        if (visible && mesh != nullptr && mesh->mesh() != MeshComponent::no_mesh())
        {
            // Entity value doubles as the occlusion-culling identity
            // (RenderItem::occlusion_id) — stable across frames as long as
            // the scene's entity set itself doesn't change shape, which
            // holds for every JSON-authored scene this engine loads (no
            // runtime entity add/remove, only component value changes).
            RenderItem item(mesh->mesh(), world);
            item.set_occlusion_id(entity.value());
            out_items->push_back(item);
        }
    }
}

bool Scene::set_visible(const std::string &name, bool visible)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    _registry.emplace<VisibilityComponent>(it->second, VisibilityComponent(visible));
    return true;
}

bool Scene::toggle_visible(const std::string &name)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    VisibilityComponent *visibility = _registry.try_get<VisibilityComponent>(it->second);
    bool new_value = (visibility == nullptr) || !visibility->visible();
    _registry.emplace<VisibilityComponent>(it->second, VisibilityComponent(new_value));
    return true;
}

bool Scene::is_visible(const std::string &name) const
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    const VisibilityComponent *visibility = _registry.try_get<VisibilityComponent>(it->second);
    return (visibility == nullptr) || visibility->visible();
}

bool Scene::set_node_rotation(const std::string &name, const vec3 &euler_radians)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    TransformComponent *transform = _registry.try_get<TransformComponent>(it->second);
    if (transform == nullptr)
        return false;
    transform->set_rotation(euler_radians);
    return true;
}

bool Scene::get_node_position(const std::string &name, vec3 *out_position) const
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;

    // World position, not local: walks the parent chain the same way
    // collect_render_items() does, so proximity checks against a child of
    // a moved/rotated parent are still correct.
    ecs::Entity entity = it->second;
    const TransformComponent *transform = _registry.try_get<TransformComponent>(entity);
    mat4 world = (transform != nullptr) ? local_transform(*transform) : mat4::identity();

    const ParentComponent *parent = _registry.try_get<ParentComponent>(entity);
    while (parent != nullptr && parent->parent() != ecs::Entity::invalid())
    {
        const TransformComponent *parent_transform =
            _registry.try_get<TransformComponent>(parent->parent());
        mat4 parent_local = (parent_transform != nullptr)
            ? local_transform(*parent_transform) : mat4::identity();
        world = mat4::multiply(parent_local, world);
        parent = _registry.try_get<ParentComponent>(parent->parent());
    }
    *out_position = vec3(world.m(12), world.m(13), world.m(14));
    return true;
}

const std::vector<Light> &Scene::get_lights() const
{
    return (_lights);
}

float Scene::get_ambient() const
{
    return (_ambient);
}

size_t Scene::get_light_count() const
{
    return (_lights.size());
}

bool Scene::set_light_intensity(size_t index, float intensity)
{
    if (index >= _lights.size())
        return false;
    _lights[index].set_intensity(intensity);
    return true;
}

} // namespace vre
