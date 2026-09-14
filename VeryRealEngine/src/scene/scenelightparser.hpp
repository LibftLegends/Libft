/**
 * @file scenelightparser.hpp
 * @brief Parses a scene file's top-level "ambient" field and "lights"
 * array. Extracted out of Scene::load() as a genuine parsing
 * responsibility distinct from the entity/registry bookkeeping load()
 * itself owns.
 */
#pragma once

#include "../assets/jsonvalue.hpp"
#include "../renderer/light.hpp"
#include <vector>

namespace vre
{
class SceneLightParser
{
	public:
		SceneLightParser();
		~SceneLightParser();

		/**
		 * @brief Reads `root`'s optional "ambient" field and "lights"
		 * array.
		 * @param root The scene file's top-level JSON object.
		 * @param out_lights Parsed lights are appended here.
		 * @param out_ambient Overwritten if `root` has an "ambient" field.
		 */
		void parse(const JsonValue &root, std::vector<Light> *out_lights,
			float *out_ambient) const;

	private:
		SceneLightParser(const SceneLightParser &other);
		SceneLightParser &operator=(const SceneLightParser &other);
};

} // namespace vre
