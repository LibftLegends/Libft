#include "skinned_mesh_loader.hpp"
#include "json_parser.hpp"

#include <cstdio>

namespace vre
{

namespace
{

vec3 read_vec3(const JsonValue *value, const vec3 &default_value)
{
    if (value == nullptr || !value->is_array() || value->array_value.size() != 3)
        return default_value;
    return vec3(
        static_cast<float>(value->array_value[0].as_number()),
        static_cast<float>(value->array_value[1].as_number()),
        static_cast<float>(value->array_value[2].as_number()));
}

bool read_vec4_floats(const JsonValue *value, float out[4], float default_value)
{
    for (int i = 0; i < 4; i++)
        out[i] = default_value;
    if (value == nullptr || !value->is_array())
        return false;
    for (size_t i = 0; i < value->array_value.size() && i < 4; i++)
        out[i] = static_cast<float>(value->array_value[i].as_number());
    return true;
}

} // namespace

bool load_skinned_asset(const char *path, SkinnedAsset *out_asset)
{
    JsonValue root;
    if (!load_json_file(path, &root))
    {
        std::fprintf(stderr, "load_skinned_asset: could not parse \"%s\" as JSON\n", path);
        return false;
    }

    const JsonValue *bones_field = root.find("bones");
    const JsonValue *vertices_field = root.find("vertices");
    const JsonValue *indices_field = root.find("indices");
    if (bones_field == nullptr || !bones_field->is_array()
        || vertices_field == nullptr || !vertices_field->is_array()
        || indices_field == nullptr || !indices_field->is_array())
    {
        std::fprintf(stderr,
            "load_skinned_asset: \"%s\" is missing a \"bones\", \"vertices\", or "
            "\"indices\" array\n", path);
        return false;
    }

    SkinnedAsset asset;

    for (const JsonValue &bone_json : bones_field->array_value)
    {
        Bone bone;
        bone.name = bone_json.find("name") != nullptr
            ? bone_json.find("name")->as_string() : std::string();
        const JsonValue *parent_field = bone_json.find("parent");
        bone.parent_index = (parent_field != nullptr)
            ? static_cast<int32_t>(parent_field->as_number(-1)) : kNoParentBone;
        bone.bind_local_position = read_vec3(bone_json.find("position"), vec3(0.0f, 0.0f, 0.0f));
        bone.bind_local_rotation = read_vec3(bone_json.find("rotation"), vec3(0.0f, 0.0f, 0.0f));
        asset.skeleton.bones.push_back(bone);
    }
    asset.skeleton.compute_bind_pose();

    // Every local bone index is offset by +1 when stored in a MeshVertex:
    // global bone slot 0 is permanently the identity matrix (see
    // mesh_data.hpp's MeshVertex doc comment and Renderer::update_global_ubo),
    // so this asset's own bones — indexed 0-based here and in its animation
    // tracks below — occupy global slots [1, bone_count].
    for (const JsonValue &vertex_json : vertices_field->array_value)
    {
        MeshVertex vertex{};
        vec3 position = read_vec3(vertex_json.find("position"), vec3(0.0f, 0.0f, 0.0f));
        vec3 normal = read_vec3(vertex_json.find("normal"), vec3(0.0f, 1.0f, 0.0f));
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.position[2] = position.z;
        vertex.normal[0] = normal.x;
        vertex.normal[1] = normal.y;
        vertex.normal[2] = normal.z;

        const JsonValue *uv_field = vertex_json.find("uv");
        if (uv_field != nullptr && uv_field->is_array() && uv_field->array_value.size() == 2)
        {
            vertex.uv[0] = static_cast<float>(uv_field->array_value[0].as_number());
            vertex.uv[1] = static_cast<float>(uv_field->array_value[1].as_number());
        }

        float bone_indices[4];
        float bone_weights[4];
        read_vec4_floats(vertex_json.find("bone_indices"), bone_indices, 0.0f);
        bool has_weights = read_vec4_floats(vertex_json.find("bone_weights"), bone_weights, 0.0f);
        if (!has_weights)
            bone_weights[0] = 1.0f; // default: fully bound to bone 0 (see comment above)
        for (int i = 0; i < 4; i++)
            vertex.bone_indices[i] = bone_indices[i] + 1.0f; // +1: see comment above
        for (int i = 0; i < 4; i++)
            vertex.bone_weights[i] = bone_weights[i];

        asset.mesh.vertices.push_back(vertex);
    }

    for (const JsonValue &index_json : indices_field->array_value)
        asset.mesh.indices.push_back(static_cast<uint32_t>(index_json.as_number()));

    SubMesh submesh;
    submesh.index_offset = 0;
    submesh.index_count = static_cast<uint32_t>(asset.mesh.indices.size());
    submesh.material_index = -1; // untextured default material — see obj_loader.cpp's own convention
    asset.mesh.submeshes.push_back(submesh);

    const JsonValue *animation_field = root.find("animation");
    if (animation_field != nullptr && animation_field->is_object())
    {
        asset.clip.duration = static_cast<float>(animation_field->find("duration") != nullptr
            ? animation_field->find("duration")->as_number(1.0) : 1.0);

        const JsonValue *tracks_field = animation_field->find("tracks");
        if (tracks_field != nullptr && tracks_field->is_array())
        {
            for (const JsonValue &track_json : tracks_field->array_value)
            {
                AnimationTrack track;
                track.bone_index = static_cast<uint32_t>(
                    track_json.find("bone") != nullptr ? track_json.find("bone")->as_number(0) : 0);

                const JsonValue *keyframes_field = track_json.find("keyframes");
                if (keyframes_field != nullptr && keyframes_field->is_array())
                {
                    for (const JsonValue &keyframe_json : keyframes_field->array_value)
                    {
                        Keyframe keyframe;
                        keyframe.time = static_cast<float>(keyframe_json.find("time") != nullptr
                            ? keyframe_json.find("time")->as_number(0.0) : 0.0);
                        keyframe.rotation = read_vec3(keyframe_json.find("rotation"),
                            vec3(0.0f, 0.0f, 0.0f));
                        track.keyframes.push_back(keyframe);
                    }
                }
                asset.clip.tracks.push_back(track);
            }
        }
    }

    std::fprintf(stderr,
        "Renderer: loaded skinned asset \"%s\": %zu bone(s), %zu vertices, %zu indices, "
        "%zu animation track(s)\n",
        path, asset.skeleton.bones.size(), asset.mesh.vertices.size(),
        asset.mesh.indices.size(), asset.clip.tracks.size());

    *out_asset = std::move(asset);
    return true;
}

} // namespace vre
