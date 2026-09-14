#include "objectloader.hpp"

namespace vre
{
SceneObjectLoader::SceneObjectLoader()
{
}

SceneObjectLoader::~SceneObjectLoader()
{
}

bool SceneObjectLoader::load_object(const JsonValue &object_json,
	const char *path, Renderer *renderer, PhysicsWorld *physics_world,
	ecs::Registry *registry,
	std::map<std::string, ecs::Entity> *name_to_entity,
	std::vector<ecs::Entity> *load_order) const
{
	std::string name = object_json.find("name") != nullptr
		? object_json.find("name")->as_string() : std::string();
	if (name.empty())
	{
		std::fprintf(stderr,
			"Scene: \"%s\" has an object with no \"name\"\n", path);
		return (false);
	}

	ecs::Entity entity = registry->create();
	registry->emplace<NameComponent>(entity, NameComponent(name));

	ecs::Entity parent_entity = ecs::Entity::invalid();
	const JsonValue *parent_field = object_json.find("parent");
	if (parent_field != nullptr && parent_field->type() == JsonType::String)
	{
		auto parent_it = name_to_entity->find(parent_field->string_value());
		if (parent_it == name_to_entity->end())
		{
			std::fprintf(stderr,
				"Scene: \"%s\": object \"%s\" references parent \"%s\", "
				"which must appear earlier in \"objects\"\n",
				path, name.c_str(), parent_field->string_value().c_str());
			return (false);
		}
		parent_entity = parent_it->second;
		registry->emplace<ParentComponent>(entity,
			ParentComponent(parent_entity));
	}

	MeshHandle mesh = MeshComponent::no_mesh();
	const JsonValue *mesh_field = object_json.find("mesh");
	if (mesh_field != nullptr && mesh_field->type() == JsonType::String)
		mesh = renderer->load_mesh_from_obj(mesh_field->string_value().c_str());
	if (mesh != MeshComponent::no_mesh())
		registry->emplace<MeshComponent>(entity, MeshComponent(mesh));

	const JsonValue *position_field = object_json.find("position");
	const JsonValue *rotation_field = object_json.find("rotation");
	const JsonValue *scale_field = object_json.find("scale");
	const JsonValue *spin_field = object_json.find("spin");
	TransformComponent transform;
	transform.set_position((position_field != nullptr)
		? position_field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
		: vec3(0.0f, 0.0f, 0.0f));
	transform.set_rotation((rotation_field != nullptr)
		? rotation_field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
		: vec3(0.0f, 0.0f, 0.0f));
	transform.set_scale((scale_field != nullptr)
		? scale_field->as_vec3(vec3(1.0f, 1.0f, 1.0f))
		: vec3(1.0f, 1.0f, 1.0f));
	transform.set_spin((spin_field != nullptr)
		? spin_field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
		: vec3(0.0f, 0.0f, 0.0f));
	registry->emplace<TransformComponent>(entity, transform);

	const JsonValue *visible_field = object_json.find("visible");
	bool visible = (visible_field != nullptr)
		? visible_field->as_bool(true) : true;
	registry->emplace<VisibilityComponent>(entity,
		VisibilityComponent(visible));

	if (physics_world != nullptr)
	{
		RigidBodyDesc physics_desc;
		if (_physics_parser.parse(object_json, name, transform.position(),
			&physics_desc))
		{
			if (parent_entity != ecs::Entity::invalid())
			{
				std::fprintf(stderr,
					"Scene: \"%s\": object \"%s\" has both \"physics\" and "
					"\"parent\" — physics-driven nodes must be root nodes, "
					"ignoring physics\n",
					path, name.c_str());
			}
			else
			{
				BodyHandle body = physics_world->add_body(physics_desc);
				registry->emplace<PhysicsBodyComponent>(entity,
					PhysicsBodyComponent(body));
			}
		}
	}

	(*name_to_entity)[name] = entity;
	load_order->push_back(entity);
	return (true);
}

} // namespace vre
