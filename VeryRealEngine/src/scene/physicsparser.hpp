/**
 * @file physicsparser.hpp
 * @brief Parses a scene object's optional "physics" JSON field into a
 * RigidBodyDesc. Extracted out of Scene::load() as a genuine parsing
 * responsibility distinct from the entity/registry bookkeeping load()
 * itself owns.
 */
#pragma once

#include "../json/value.hpp"
#include "../math/vec3.hpp"
#include "../physics/rigidbodydesc.hpp"
#include <string>

namespace vre
{
class ScenePhysicsParser
{
	public:
		ScenePhysicsParser();
		~ScenePhysicsParser();

		/**
		 * @brief Parses `object_json`'s "physics" field, if present.
		 * @param object_json The scene object's JSON node.
		 * @param name Object name, stored on the desc for debugging.
		 * @param position World position, stored on the desc.
		 * @param out_desc Receives the parsed body description.
		 * @return false if `object_json` has no "physics" field.
		 */
		bool parse(const JsonValue &object_json, const std::string &name,
			const vec3 &position, RigidBodyDesc *out_desc) const;

	private:
		ScenePhysicsParser(const ScenePhysicsParser &other);
		ScenePhysicsParser &operator=(const ScenePhysicsParser &other);
};

} // namespace vre
