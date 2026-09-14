/**
 * @file skinned_mesh_grammar.cpp
 * @brief SkinnedMeshLoader's per-JSON-section read helpers, split out of
 * skinned_mesh_loader.cpp purely to keep each file under this project's
 * 250-line cap — both files define methods of the same SkinnedMeshLoader
 * class declared in skinned_mesh_loader.hpp.
 */
#include "skinned_mesh_loader.hpp"

namespace vre
{

void SkinnedMeshLoader::read_vertex(const JsonValue &vertex_json, SkinnedAsset *out_asset)
{
    // Every local bone index is offset by +1 when stored in a MeshVertex:
    // global bone slot 0 is permanently the identity matrix (see
    // mesh_vertex.hpp's doc comment and Renderer::update_global_ubo), so
    // this asset's own bones — indexed 0-based here and in its animation
    // tracks — occupy global slots [1, bone_count].
    MeshVertex vertex;
    vec3 position = vertex_json.find("position") != nullptr
        ? vertex_json.find("position")->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f);
    vec3 normal = vertex_json.find("normal") != nullptr
        ? vertex_json.find("normal")->as_vec3(vec3(0.0f, 1.0f, 0.0f)) : vec3(0.0f, 1.0f, 0.0f);
    vertex.set_position(0, position.x());
    vertex.set_position(1, position.y());
    vertex.set_position(2, position.z());
    vertex.set_normal(0, normal.x());
    vertex.set_normal(1, normal.y());
    vertex.set_normal(2, normal.z());

    const JsonValue *uv_field = vertex_json.find("uv");
    if (uv_field != nullptr && uv_field->is_array() && uv_field->array_elements().size() == 2)
    {
        vertex.set_uv(0, static_cast<float>(uv_field->array_elements()[0].as_number()));
        vertex.set_uv(1, static_cast<float>(uv_field->array_elements()[1].as_number()));
    }

    float bone_indices[4];
    float bone_weights[4];
    read_vec4_floats(vertex_json.find("bone_indices"), bone_indices, 0.0f);
    bool has_weights = read_vec4_floats(vertex_json.find("bone_weights"), bone_weights, 0.0f);
    if (!has_weights)
        bone_weights[0] = 1.0f; // default: fully bound to bone 0 (see comment above)
    for (int i = 0; i < 4; i++)
        vertex.set_bone_index(i, bone_indices[i] + 1.0f); // +1: see comment above
    for (int i = 0; i < 4; i++)
        vertex.set_bone_weight(i, bone_weights[i]);

    out_asset->mesh().vertices().push_back(vertex);
}

bool SkinnedMeshLoader::read_animation(const JsonValue &root, SkinnedAsset *out_asset)
{
    const JsonValue *animation_field = root.find("animation");
    if (animation_field == nullptr || !animation_field->is_object())
        return (false);

    out_asset->clip().duration = static_cast<float>(animation_field->find("duration") != nullptr
        ? animation_field->find("duration")->as_number(1.0) : 1.0);

    const JsonValue *tracks_field = animation_field->find("tracks");
    if (tracks_field == nullptr || !tracks_field->is_array())
        return (true);

    for (const JsonValue &track_json : tracks_field->array_elements())
    {
        AnimationTrack track;
        track.bone_index = static_cast<uint32_t>(
            track_json.find("bone") != nullptr ? track_json.find("bone")->as_number(0) : 0);

        const JsonValue *keyframes_field = track_json.find("keyframes");
        if (keyframes_field != nullptr && keyframes_field->is_array())
        {
            for (const JsonValue &keyframe_json : keyframes_field->array_elements())
            {
                Keyframe keyframe;
                keyframe.time = static_cast<float>(keyframe_json.find("time") != nullptr
                    ? keyframe_json.find("time")->as_number(0.0) : 0.0);
                keyframe.rotation = keyframe_json.find("rotation") != nullptr
                    ? keyframe_json.find("rotation")->as_vec3(vec3(0.0f, 0.0f, 0.0f))
                    : vec3(0.0f, 0.0f, 0.0f);
                track.keyframes.push_back(keyframe);
            }
        }
        out_asset->clip().tracks.push_back(track);
    }
    return (true);
}

} // namespace vre
