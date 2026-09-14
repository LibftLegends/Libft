/**
 * @file skinned_mesh_loader.hpp
 * @brief Loads this project's own hand-written JSON format for an animated,
 * skinned rig: a bone hierarchy, a weighted mesh, and one looping animation
 * clip, all in one file.
 *
 * OBJ/MTL has no concept of bone weights, so — exactly as the subject
 * permits ("You may create your own custom file type") — this is a small,
 * separate, from-scratch JSON schema for skinned content specifically,
 * reusing the engine's existing hand-written JSON parser (json_parser.hpp)
 * rather than inventing a new text format from scratch.
 */
#pragma once

#include "../vre.hpp"
#include "json_value.hpp"
#include "skinned_asset.hpp"

namespace vre
{

class SkinnedMeshLoader
{
  public:
	SkinnedMeshLoader();
	SkinnedMeshLoader(const SkinnedMeshLoader &other);
	SkinnedMeshLoader &operator=(const SkinnedMeshLoader &other);
	~SkinnedMeshLoader();

	/**
		* @brief Loads a skinned-rig JSON file (see this project's
		* `assets/models` directory for a real example of the schema:
		* top-level `"bones"`, `"vertices"` (each with
		* `"bone_indices"`/`"bone_weights"`), `"indices"`, and `"animation"`).
		* @param path Filesystem path to the JSON file.
		* @param out_asset Filled on success; left untouched on failure.
		* @return true on success, false if the file is missing or malformed.
		*/
	static bool load(const char *path, SkinnedAsset *out_asset);

  private:
	static bool read_bones(const JsonValue &bones_field,
		SkinnedAsset *out_asset);
	static void read_vertex(const JsonValue &vertex_json,
		SkinnedAsset *out_asset);
	static bool read_animation(const JsonValue &root, SkinnedAsset *out_asset);
	/** @return `value` parsed as up to 4 floats,
		or all `default_value` if malformed. */
	static bool read_vec4_floats(const JsonValue *value, float out[4],
		float default_value);
};

} // namespace vre
