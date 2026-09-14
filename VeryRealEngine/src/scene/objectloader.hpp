/**
 * @file objectloader.hpp
 * @brief Builds one ECS entity from a single element of a scene file's
 * "objects" array: name, parent reference, mesh, transform, visibility
 * and optional physics body. Extracted out of Scene::load(), which keeps
 * only the top-level format checks and the loop driving this per-object
 * work.
 */
#pragma once

#include "../ecs/registry.hpp"
#include "../physics/physicsworld.hpp"
#include "../renderer/renderer.hpp"
#include "../components/mesh.hpp"
#include "../components/name.hpp"
#include "../components/parent.hpp"
#include "../components/physicsbody.hpp"
#include "physicsparser.hpp"
#include "../components/transform.hpp"
#include "../components/visibility.hpp"
#include <map>
#include <string>
#include <vector>

namespace vre
{
class SceneObjectLoader
{
	public:
		SceneObjectLoader();
		~SceneObjectLoader();

		/**
		 * @brief Creates and registers one entity from `object_json`.
		 * @param object_json One element of the "objects" array.
		 * @param path Scene file path, used only for error messages.
		 * @param renderer Renderer used to load a referenced mesh, if any.
		 * @param physics_world If non-null, a rigid body is created here
		 * for a root-node "physics" field.
		 * @param registry Component storage the new entity is created in.
		 * @param name_to_entity Updated with the new entity's name.
		 * @param load_order Appended with the new entity, in call order.
		 * @return false if `object_json` is malformed (missing/duplicate
		 * name, or a parent that hasn't been loaded yet).
		 */
		bool load_object(const JsonValue &object_json, const char *path,
			Renderer *renderer, PhysicsWorld *physics_world,
			ecs::Registry *registry,
			std::map<std::string, ecs::Entity> *name_to_entity,
			std::vector<ecs::Entity> *load_order) const;

	private:
		SceneObjectLoader(const SceneObjectLoader &other);
		SceneObjectLoader &operator=(const SceneObjectLoader &other);

		ScenePhysicsParser _physics_parser;
};

} // namespace vre
