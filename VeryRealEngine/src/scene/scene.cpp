#include "scene.hpp"
#include "../assets/json_parser.hpp"

#include <cstdio>

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

static bool parse_physics(const JsonValue &object_json, const SceneNode &node,
    RigidBodyDesc *out_desc)
{
    const JsonValue *physics_field = object_json.find("physics");
    if (physics_field == nullptr || !physics_field->is_object())
        return false;

    out_desc->position = node.position;
    out_desc->debug_name = node.name;

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
        SceneNode node;
        node.name = object_json.find("name") != nullptr
            ? object_json.find("name")->as_string() : std::string();
        if (node.name.empty())
        {
            std::fprintf(stderr, "Scene: \"%s\" has an object with no \"name\"\n", path);
            return false;
        }

        const JsonValue *parent_field = object_json.find("parent");
        if (parent_field != nullptr && parent_field->type == JsonType::String)
        {
            auto parent_it = _name_to_index.find(parent_field->string_value);
            if (parent_it == _name_to_index.end())
            {
                std::fprintf(stderr,
                    "Scene: \"%s\": object \"%s\" references parent \"%s\", which must "
                    "appear earlier in \"objects\"\n",
                    path, node.name.c_str(), parent_field->string_value.c_str());
                return false;
            }
            node.parent_index = parent_it->second;
        }

        const JsonValue *mesh_field = object_json.find("mesh");
        if (mesh_field != nullptr && mesh_field->type == JsonType::String)
            node.mesh = renderer->load_mesh_from_obj(mesh_field->string_value.c_str());

        node.position = read_vec3(object_json.find("position"), vec3(0.0f, 0.0f, 0.0f));
        node.rotation = read_vec3(object_json.find("rotation"), vec3(0.0f, 0.0f, 0.0f));
        node.scale = read_vec3(object_json.find("scale"), vec3(1.0f, 1.0f, 1.0f));
        node.spin = read_vec3(object_json.find("spin"), vec3(0.0f, 0.0f, 0.0f));

        const JsonValue *visible_field = object_json.find("visible");
        node.visible = (visible_field != nullptr) ? visible_field->as_bool(true) : true;

        if (physics_world != nullptr)
        {
            RigidBodyDesc physics_desc;
            if (parse_physics(object_json, node, &physics_desc))
            {
                if (node.parent_index != kNoParent)
                {
                    std::fprintf(stderr,
                        "Scene: \"%s\": object \"%s\" has both \"physics\" and \"parent\" — "
                        "physics-driven nodes must be root nodes, ignoring physics\n",
                        path, node.name.c_str());
                }
                else
                {
                    node.physics_body = physics_world->add_body(physics_desc);
                }
            }
        }

        _name_to_index[node.name] = _nodes.size();
        _nodes.push_back(node);
    }

    std::fprintf(stderr, "Scene: loaded \"%s\": %zu node(s)\n", path, _nodes.size());
    return true;
}

void Scene::sync_from_physics(const PhysicsWorld &physics_world)
{
    for (auto &node : _nodes)
    {
        if (node.physics_body != kInvalidBody)
            node.position = physics_world.get_position(node.physics_body);
    }
}

void Scene::update(float delta_seconds)
{
    for (auto &node : _nodes)
    {
        node.spin_accumulated = node.spin_accumulated + node.spin * delta_seconds;
    }
}

mat4 Scene::local_transform(const SceneNode &node) const
{
    vec3 total_rotation = node.rotation + node.spin_accumulated;
    return mat4::compose(node.position, total_rotation, node.scale);
}

void Scene::collect_render_items(std::vector<RenderItem> *out_items) const
{
    std::vector<mat4> world_transforms(_nodes.size());
    std::vector<bool> world_visible(_nodes.size());

    // Requires parents to precede children in _nodes (enforced at load
    // time), so a single forward pass suffices — no recursion needed.
    for (size_t i = 0; i < _nodes.size(); i++)
    {
        const SceneNode &node = _nodes[i];
        mat4 local = local_transform(node);

        if (node.parent_index == kNoParent)
        {
            world_transforms[i] = local;
            world_visible[i] = node.visible;
        }
        else
        {
            world_transforms[i] = mat4::multiply(world_transforms[node.parent_index], local);
            world_visible[i] = node.visible && world_visible[node.parent_index];
        }

        if (world_visible[i] && node.mesh != kNoMesh)
            out_items->push_back(RenderItem{node.mesh, world_transforms[i]});
    }
}

bool Scene::set_visible(const std::string &name, bool visible)
{
    auto it = _name_to_index.find(name);
    if (it == _name_to_index.end())
        return false;
    _nodes[it->second].visible = visible;
    return true;
}

bool Scene::toggle_visible(const std::string &name)
{
    auto it = _name_to_index.find(name);
    if (it == _name_to_index.end())
        return false;
    SceneNode &node = _nodes[it->second];
    node.visible = !node.visible;
    return true;
}

bool Scene::is_visible(const std::string &name) const
{
    auto it = _name_to_index.find(name);
    if (it == _name_to_index.end())
        return false;
    return _nodes[it->second].visible;
}

} // namespace vre
