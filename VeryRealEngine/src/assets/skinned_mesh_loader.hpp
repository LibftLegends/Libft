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

#include "mesh_data.hpp"
#include "../animation/skeleton.hpp"

namespace vre
{

/// Everything one skinned asset needs: geometry plus the skeleton/clip that animates it.
struct SkinnedAsset
{
    MeshData mesh;
    Skeleton skeleton;
    AnimationClip clip;
};

/**
 * @brief Loads a skinned-rig JSON file (see this project's
 * `assets/models` directory for a real example of the schema: top-level
 * `"bones"`, `"vertices"` (each with `"bone_indices"`/`"bone_weights"`),
 * `"indices"`, and `"animation"`).
 * @param path Filesystem path to the JSON file.
 * @param out_asset Filled on success; left untouched on failure.
 * @return true on success, false if the file is missing or malformed.
 */
bool load_skinned_asset(const char *path, SkinnedAsset *out_asset);

} // namespace vre
