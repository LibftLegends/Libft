#include "scene.hpp"

namespace vre
{
Scene::Scene() : _ambient(0.12f)
{
}

Scene::~Scene()
{
}

bool Scene::load(const char *path, Renderer *renderer,
	PhysicsWorld *physics_world)
{
	JsonValue root;
	if (!JsonParser::load_file(path, &root))
		return (false);

	const JsonValue *objects = root.find("objects");
	if (objects == nullptr || !objects->is_array())
	{
		std::fprintf(stderr,
			"Scene: \"%s\" has no top-level \"objects\" array\n", path);
		return (false);
	}

	_light_parser.parse(root, &_lights, &_ambient);

	for (const JsonValue &object_json : objects->array_elements())
	{
		if (!_object_loader.load_object(object_json, path, renderer,
			physics_world, &_registry, &_name_to_entity, &_load_order))
			return (false);
	}

	std::fprintf(stderr, "Scene: loaded \"%s\": %zu node(s)\n",
		path, _load_order.size());
	return (true);
}

void Scene::sync_from_physics(const PhysicsWorld &physics_world)
{
	for (auto &[entity, body] : _registry.storage<PhysicsBodyComponent>())
	{
		TransformComponent *transform =
			_registry.try_get<TransformComponent>(entity);
		if (transform != nullptr && body.body() != BodyHandle::invalid())
			transform->set_position(physics_world.get_position(body.body()));
	}
}

void Scene::update(float delta_seconds)
{
	for (auto &[entity, transform] : _registry.storage<TransformComponent>())
	{
		(void)entity;
		transform.set_spin_accumulated(
			transform.spin_accumulated() + transform.spin() * delta_seconds);
	}
}

void Scene::collect_render_items(std::vector<RenderItem> *out_items) const
{
	// const_cast is safe/contained here: WorldTransformComponent is a
	// frame-local cache, not scene-authored state, and recomputing it is
	// exactly what a const "give me this frame's render items" call means.
	auto &registry = const_cast<ecs::Registry &>(_registry);
	_render_collector.collect(registry, _load_order, out_items);
}

bool Scene::set_visible(const std::string &name, bool visible)
{
	auto it = _name_to_entity.find(name);
	if (it == _name_to_entity.end())
		return (false);
	_registry.emplace<VisibilityComponent>(it->second,
		VisibilityComponent(visible));
	return (true);
}

bool Scene::toggle_visible(const std::string &name)
{
	auto it = _name_to_entity.find(name);
	if (it == _name_to_entity.end())
		return (false);
	VisibilityComponent *visibility =
		_registry.try_get<VisibilityComponent>(it->second);
	bool new_value = (visibility == nullptr) || !visibility->visible();
	_registry.emplace<VisibilityComponent>(it->second,
		VisibilityComponent(new_value));
	return (true);
}

bool Scene::is_visible(const std::string &name) const
{
	auto it = _name_to_entity.find(name);
	if (it == _name_to_entity.end())
		return (false);
	const VisibilityComponent *visibility =
		_registry.try_get<VisibilityComponent>(it->second);
	return ((visibility == nullptr) || visibility->visible());
}

bool Scene::set_node_rotation(const std::string &name,
	const vec3 &euler_radians)
{
	auto it = _name_to_entity.find(name);
	if (it == _name_to_entity.end())
		return (false);
	TransformComponent *transform =
		_registry.try_get<TransformComponent>(it->second);
	if (transform == nullptr)
		return (false);
	transform->set_rotation(euler_radians);
	return (true);
}

bool Scene::get_node_position(const std::string &name,
	vec3 *out_position) const
{
	auto it = _name_to_entity.find(name);
	if (it == _name_to_entity.end())
		return (false);

	// World position, not local: walks the parent chain the same way
	// collect_render_items() does, so proximity checks against a child of
	// a moved/rotated parent are still correct.
	ecs::Entity entity = it->second;
	const TransformComponent *transform =
		_registry.try_get<TransformComponent>(entity);
	mat4 world = (transform != nullptr)
		? _transform_composer.compose_local(*transform) : mat4::identity();

	const ParentComponent *parent = _registry.try_get<ParentComponent>(entity);
	while (parent != nullptr && parent->parent() != ecs::Entity::invalid())
	{
		const TransformComponent *parent_transform =
			_registry.try_get<TransformComponent>(parent->parent());
		mat4 parent_local = (parent_transform != nullptr)
			? _transform_composer.compose_local(*parent_transform)
			: mat4::identity();
		world = mat4::multiply(parent_local, world);
		parent = _registry.try_get<ParentComponent>(parent->parent());
	}
	*out_position = vec3(world.m(12), world.m(13), world.m(14));
	return (true);
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
		return (false);
	_lights[index].set_intensity(intensity);
	return (true);
}

} // namespace vre
