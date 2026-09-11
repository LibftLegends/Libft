#include "scene.hpp"
#include "../assets/json_parser.hpp"

#include <cstdio>
#include <unordered_map>

namespace vre
{

static vec3 read_vec3(const JsonValue *value, const vec3 &default_value)
{
    if (value == nullptr || !value->is_array() || value->array_value.size() != 3)
        return default_value;
    return vec3(
        static_cast<float>(value->array_value[0].as_number()),
        static_cast<float>(value->array_value[1].as_number()),
        static_cast<float>(value->array_value[2].as_number()));
}

static bool parse_physics(const JsonValue &object_json, const std::string &name,
    const vec3 &position, RigidBodyDesc *out_desc)
{
    const JsonValue *physics_field = object_json.find("physics");
    if (physics_field == nullptr || !physics_field->is_object())
        return false;

    out_desc->position = position;
    out_desc->debug_name = name;

    const JsonValue *static_field = physics_field->find("static");
    out_desc->is_static = (static_field != nullptr) ? static_field->as_bool(false) : false;

    const JsonValue *trigger_field = physics_field->find("trigger");
    out_desc->is_trigger = (trigger_field != nullptr) ? trigger_field->as_bool(false) : false;

    const JsonValue *mass_field = physics_field->find("mass");
    out_desc->mass = static_cast<float>((mass_field != nullptr) ? mass_field->as_number(1.0) : 1.0);

    const JsonValue *restitution_field = physics_field->find("restitution");
    out_desc->restitution = static_cast<float>(
        (restitution_field != nullptr) ? restitution_field->as_number(0.3) : 0.3);

    const JsonValue *friction_field = physics_field->find("friction");
    out_desc->friction = static_cast<float>(
        (friction_field != nullptr) ? friction_field->as_number(0.5) : 0.5);

    out_desc->velocity = read_vec3(physics_field->find("initial_velocity"), vec3(0.0f, 0.0f, 0.0f));

    std::string shape = physics_field->find("collider") != nullptr
        ? physics_field->find("collider")->as_string("box") : "box";
    if (shape == "sphere")
    {
        out_desc->collider.type = ColliderType::Sphere;
        const JsonValue *radius_field = physics_field->find("radius");
        out_desc->collider.radius = static_cast<float>(
            (radius_field != nullptr) ? radius_field->as_number(0.5) : 0.5);
    }
    else
    {
        out_desc->collider.type = ColliderType::Box;
        out_desc->collider.half_extents = read_vec3(physics_field->find("half_extents"),
            vec3(0.5f, 0.5f, 0.5f));
    }

    return true;
}

bool Scene::load(const char *path, Renderer *renderer, PhysicsWorld *physics_world)
{
    JsonValue root;
    if (!load_json_file(path, &root))
        return false;

    const JsonValue *objects = root.find("objects");
    if (objects == nullptr || !objects->is_array())
    {
        std::fprintf(stderr, "Scene: \"%s\" has no top-level \"objects\" array\n", path);
        return false;
    }

    const JsonValue *ambient_field = root.find("ambient");
    if (ambient_field != nullptr)
        _ambient = static_cast<float>(ambient_field->as_number(0.12));

    const JsonValue *lights_field = root.find("lights");
    if (lights_field != nullptr && lights_field->is_array())
    {
        for (const JsonValue &light_json : lights_field->array_value)
        {
            Light light;
            std::string type_name = light_json.find("type") != nullptr
                ? light_json.find("type")->as_string("directional") : "directional";
            light.type = (type_name == "point") ? LightType::Point : LightType::Directional;

            const char *position_field_name = (light.type == LightType::Point) ? "position" : "direction";
            light.direction_or_position = read_vec3(light_json.find(position_field_name),
                vec3(0.0f, -1.0f, 0.0f));

            light.color = read_vec3(light_json.find("color"), vec3(1.0f, 1.0f, 1.0f));

            const JsonValue *intensity_field = light_json.find("intensity");
            light.intensity = static_cast<float>(
                (intensity_field != nullptr) ? intensity_field->as_number(1.0) : 1.0);

            _lights.push_back(light);
        }
    }

    for (const JsonValue &object_json : objects->array_value)
    {
        std::string name = object_json.find("name") != nullptr
            ? object_json.find("name")->as_string() : std::string();
        if (name.empty())
        {
            std::fprintf(stderr, "Scene: \"%s\" has an object with no \"name\"\n", path);
            return false;
        }

        ecs::Entity entity = _registry.create();
        _registry.emplace<NameComponent>(entity, NameComponent{name});

        ecs::Entity parent_entity = ecs::kInvalidEntity;
        const JsonValue *parent_field = object_json.find("parent");
        if (parent_field != nullptr && parent_field->type == JsonType::String)
        {
            auto parent_it = _name_to_entity.find(parent_field->string_value);
            if (parent_it == _name_to_entity.end())
            {
                std::fprintf(stderr,
                    "Scene: \"%s\": object \"%s\" references parent \"%s\", which must "
                    "appear earlier in \"objects\"\n",
                    path, name.c_str(), parent_field->string_value.c_str());
                return false;
            }
            parent_entity = parent_it->second;
            _registry.emplace<ParentComponent>(entity, ParentComponent{parent_entity});
        }

        MeshHandle mesh = kNoMesh;
        const JsonValue *mesh_field = object_json.find("mesh");
        if (mesh_field != nullptr && mesh_field->type == JsonType::String)
            mesh = renderer->load_mesh_from_obj(mesh_field->string_value.c_str());
        if (mesh != kNoMesh)
            _registry.emplace<MeshComponent>(entity, MeshComponent{mesh});

        TransformComponent transform;
        transform.position = read_vec3(object_json.find("position"), vec3(0.0f, 0.0f, 0.0f));
        transform.rotation = read_vec3(object_json.find("rotation"), vec3(0.0f, 0.0f, 0.0f));
        transform.scale = read_vec3(object_json.find("scale"), vec3(1.0f, 1.0f, 1.0f));
        transform.spin = read_vec3(object_json.find("spin"), vec3(0.0f, 0.0f, 0.0f));
        _registry.emplace<TransformComponent>(entity, transform);

        const JsonValue *visible_field = object_json.find("visible");
        bool visible = (visible_field != nullptr) ? visible_field->as_bool(true) : true;
        _registry.emplace<VisibilityComponent>(entity, VisibilityComponent{visible});

        if (physics_world != nullptr)
        {
            RigidBodyDesc physics_desc;
            if (parse_physics(object_json, name, transform.position, &physics_desc))
            {
                if (parent_entity != ecs::kInvalidEntity)
                {
                    std::fprintf(stderr,
                        "Scene: \"%s\": object \"%s\" has both \"physics\" and \"parent\" — "
                        "physics-driven nodes must be root nodes, ignoring physics\n",
                        path, name.c_str());
                }
                else
                {
                    BodyHandle body = physics_world->add_body(physics_desc);
                    _registry.emplace<PhysicsBodyComponent>(entity, PhysicsBodyComponent{body});
                }
            }
        }

        _name_to_entity[name] = entity;
        _load_order.push_back(entity);
    }

    std::fprintf(stderr, "Scene: loaded \"%s\": %zu node(s)\n", path, _load_order.size());
    return true;
}

void Scene::sync_from_physics(const PhysicsWorld &physics_world)
{
    for (auto &[entity, body] : _registry.storage<PhysicsBodyComponent>())
    {
        TransformComponent *transform = _registry.try_get<TransformComponent>(entity);
        if (transform != nullptr && body.body != kInvalidBody)
            transform->position = physics_world.get_position(body.body);
    }
}

void Scene::update(float delta_seconds)
{
    for (auto &[entity, transform] : _registry.storage<TransformComponent>())
    {
        (void)entity;
        transform.spin_accumulated = transform.spin_accumulated + transform.spin * delta_seconds;
    }
}

mat4 Scene::local_transform(const TransformComponent &transform) const
{
    vec3 total_rotation = transform.rotation + transform.spin_accumulated;
    return mat4::compose(transform.position, total_rotation, transform.scale);
}

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
        bool self_visible = (visibility == nullptr) || visibility->visible;

        mat4 local = (transform != nullptr) ? local_transform(*transform) : mat4::identity();
        const ParentComponent *parent = registry.try_get<ParentComponent>(entity);

        mat4 world;
        bool visible;
        if (parent == nullptr || parent->parent == ecs::kInvalidEntity)
        {
            world = local;
            visible = self_visible;
        }
        else
        {
            const WorldTransformComponent *parent_world =
                registry.try_get<WorldTransformComponent>(parent->parent);
            world = (parent_world != nullptr) ? mat4::multiply(parent_world->value, local) : local;
            visible = self_visible && combined_visible[parent->parent];
        }

        registry.emplace<WorldTransformComponent>(entity, WorldTransformComponent{world});
        combined_visible[entity] = visible;

        const MeshComponent *mesh = registry.try_get<MeshComponent>(entity);
        if (visible && mesh != nullptr && mesh->mesh != kNoMesh)
        {
            // Entity value doubles as the occlusion-culling identity
            // (RenderItem::occlusion_id) — stable across frames as long as
            // the scene's entity set itself doesn't change shape, which
            // holds for every JSON-authored scene this engine loads (no
            // runtime entity add/remove, only component value changes).
            RenderItem item;
            item.mesh = mesh->mesh;
            item.model = world;
            item.occlusion_id = entity;
            out_items->push_back(item);
        }
    }
}

bool Scene::set_visible(const std::string &name, bool visible)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    _registry.emplace<VisibilityComponent>(it->second, VisibilityComponent{visible});
    return true;
}

bool Scene::toggle_visible(const std::string &name)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    VisibilityComponent *visibility = _registry.try_get<VisibilityComponent>(it->second);
    bool new_value = (visibility == nullptr) || !visibility->visible;
    _registry.emplace<VisibilityComponent>(it->second, VisibilityComponent{new_value});
    return true;
}

bool Scene::is_visible(const std::string &name) const
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    const VisibilityComponent *visibility = _registry.try_get<VisibilityComponent>(it->second);
    return (visibility == nullptr) || visibility->visible;
}

bool Scene::set_node_rotation(const std::string &name, const vec3 &euler_radians)
{
    auto it = _name_to_entity.find(name);
    if (it == _name_to_entity.end())
        return false;
    TransformComponent *transform = _registry.try_get<TransformComponent>(it->second);
    if (transform == nullptr)
        return false;
    transform->rotation = euler_radians;
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
    while (parent != nullptr && parent->parent != ecs::kInvalidEntity)
    {
        const TransformComponent *parent_transform =
            _registry.try_get<TransformComponent>(parent->parent);
        mat4 parent_local = (parent_transform != nullptr)
            ? local_transform(*parent_transform) : mat4::identity();
        world = mat4::multiply(parent_local, world);
        parent = _registry.try_get<ParentComponent>(parent->parent);
    }
    *out_position = vec3(world.m[12], world.m[13], world.m[14]);
    return true;
}

bool Scene::set_light_intensity(size_t index, float intensity)
{
    if (index >= _lights.size())
        return false;
    _lights[index].intensity = intensity;
    return true;
}

} // namespace vre
