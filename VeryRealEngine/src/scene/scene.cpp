#include "scene.hpp"
#include "../assets/json_parser.hpp"
#include "../renderer/renderer.hpp"

namespace vre
{

Scene::Scene() : _ambient(0.12f)
{
}

Scene::~Scene()
{
}

static bool parse_physics(const JsonValue &object_json, const std::string &name,
    const vec3 &position, RigidBodyDesc *out_desc)
{
    const JsonValue *physics_field = object_json.find("physics");
    if (physics_field == nullptr || !physics_field->is_object())
        return false;

    out_desc->set_position(position);
    out_desc->set_debug_name(name);

    const JsonValue *static_field = physics_field->find("static");
    out_desc->set_static((static_field != nullptr) ? static_field->as_bool(false) : false);

    const JsonValue *trigger_field = physics_field->find("trigger");
    out_desc->set_trigger((trigger_field != nullptr) ? trigger_field->as_bool(false) : false);

    const JsonValue *mass_field = physics_field->find("mass");
    out_desc->set_mass(static_cast<float>((mass_field != nullptr) ? mass_field->as_number(1.0) : 1.0));

    const JsonValue *restitution_field = physics_field->find("restitution");
    out_desc->set_restitution(static_cast<float>(
        (restitution_field != nullptr) ? restitution_field->as_number(0.3) : 0.3));

    const JsonValue *friction_field = physics_field->find("friction");
    out_desc->set_friction(static_cast<float>(
        (friction_field != nullptr) ? friction_field->as_number(0.5) : 0.5));

    const JsonValue *velocity_field = physics_field->find("initial_velocity");
    out_desc->set_velocity((velocity_field != nullptr)
        ? velocity_field->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));

    std::string shape = physics_field->find("collider") != nullptr
        ? physics_field->find("collider")->as_string("box") : "box";
    if (shape == "sphere")
    {
        out_desc->collider().set_type(Collider::Type::Sphere);
        const JsonValue *radius_field = physics_field->find("radius");
        out_desc->collider().set_radius(static_cast<float>(
            (radius_field != nullptr) ? radius_field->as_number(0.5) : 0.5));
    }
    else
    {
        out_desc->collider().set_type(Collider::Type::Box);
        const JsonValue *half_extents_field = physics_field->find("half_extents");
        out_desc->collider().set_half_extents((half_extents_field != nullptr)
            ? half_extents_field->as_vec3(vec3(0.5f, 0.5f, 0.5f)) : vec3(0.5f, 0.5f, 0.5f));
    }

    return true;
}

bool Scene::load(const char *path, Renderer *renderer, PhysicsWorld *physics_world)
{
    JsonValue root;
    if (!JsonParser::load_file(path, &root))
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
        for (const JsonValue &light_json : lights_field->array_elements())
        {
            Light light;
            std::string type_name = light_json.find("type") != nullptr
                ? light_json.find("type")->as_string("directional") : "directional";
            light.set_type((type_name == "point") ? Light::Type::Point : Light::Type::Directional);

            const char *position_field_name = (light.type() == Light::Type::Point)
                ? "position" : "direction";
            const JsonValue *position_field = light_json.find(position_field_name);
            light.set_direction_or_position((position_field != nullptr)
                ? position_field->as_vec3(vec3(0.0f, -1.0f, 0.0f)) : vec3(0.0f, -1.0f, 0.0f));

            const JsonValue *color_field = light_json.find("color");
            light.set_color((color_field != nullptr)
                ? color_field->as_vec3(vec3(1.0f, 1.0f, 1.0f)) : vec3(1.0f, 1.0f, 1.0f));

            const JsonValue *intensity_field = light_json.find("intensity");
            light.set_intensity(static_cast<float>(
                (intensity_field != nullptr) ? intensity_field->as_number(1.0) : 1.0));

            _lights.push_back(light);
        }
    }

    for (const JsonValue &object_json : objects->array_elements())
    {
        std::string name = object_json.find("name") != nullptr
            ? object_json.find("name")->as_string() : std::string();
        if (name.empty())
        {
            std::fprintf(stderr, "Scene: \"%s\" has an object with no \"name\"\n", path);
            return false;
        }

        ecs::Entity entity = _registry.create();
        _registry.emplace<NameComponent>(entity, NameComponent(name));

        ecs::Entity parent_entity = ecs::Entity::invalid();
        const JsonValue *parent_field = object_json.find("parent");
        if (parent_field != nullptr && parent_field->type() == JsonType::String)
        {
            auto parent_it = _name_to_entity.find(parent_field->string_value());
            if (parent_it == _name_to_entity.end())
            {
                std::fprintf(stderr,
                    "Scene: \"%s\": object \"%s\" references parent \"%s\", which must "
                    "appear earlier in \"objects\"\n",
                    path, name.c_str(), parent_field->string_value().c_str());
                return false;
            }
            parent_entity = parent_it->second;
            _registry.emplace<ParentComponent>(entity, ParentComponent(parent_entity));
        }

        MeshHandle mesh = MeshComponent::no_mesh();
        const JsonValue *mesh_field = object_json.find("mesh");
        if (mesh_field != nullptr && mesh_field->type() == JsonType::String)
            mesh = renderer->load_mesh_from_obj(mesh_field->string_value().c_str());
        if (mesh != MeshComponent::no_mesh())
            _registry.emplace<MeshComponent>(entity, MeshComponent(mesh));

        const JsonValue *position_field = object_json.find("position");
        const JsonValue *rotation_field = object_json.find("rotation");
        const JsonValue *scale_field = object_json.find("scale");
        const JsonValue *spin_field = object_json.find("spin");
        TransformComponent transform;
        transform.set_position((position_field != nullptr)
            ? position_field->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));
        transform.set_rotation((rotation_field != nullptr)
            ? rotation_field->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));
        transform.set_scale((scale_field != nullptr)
            ? scale_field->as_vec3(vec3(1.0f, 1.0f, 1.0f)) : vec3(1.0f, 1.0f, 1.0f));
        transform.set_spin((spin_field != nullptr)
            ? spin_field->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));
        _registry.emplace<TransformComponent>(entity, transform);

        const JsonValue *visible_field = object_json.find("visible");
        bool visible = (visible_field != nullptr) ? visible_field->as_bool(true) : true;
        _registry.emplace<VisibilityComponent>(entity, VisibilityComponent(visible));

        if (physics_world != nullptr)
        {
            RigidBodyDesc physics_desc;
            if (parse_physics(object_json, name, transform.position(), &physics_desc))
            {
                if (parent_entity != ecs::Entity::invalid())
                {
                    std::fprintf(stderr,
                        "Scene: \"%s\": object \"%s\" has both \"physics\" and \"parent\" — "
                        "physics-driven nodes must be root nodes, ignoring physics\n",
                        path, name.c_str());
                }
                else
                {
                    BodyHandle body = physics_world->add_body(physics_desc);
                    _registry.emplace<PhysicsBodyComponent>(entity, PhysicsBodyComponent(body));
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
        if (transform != nullptr && body.body() != BodyHandle::invalid())
            transform->set_position(physics_world.get_position(body.body()));
    }
}

void Scene::update(float delta_seconds)
{
    for (auto &[entity, transform] : _registry.storage<TransformComponent>())
    {
        (void)entity;
        transform.set_spin_accumulated(transform.spin_accumulated() + transform.spin() * delta_seconds);
    }
}

mat4 Scene::local_transform(const TransformComponent &transform) const
{
    vec3 total_rotation = transform.rotation() + transform.spin_accumulated();
    return mat4::compose(transform.position(), total_rotation, transform.scale());
}

} // namespace vre
