#include "skinned_mesh_loader.hpp"
#include "json_parser.hpp"

namespace vre
{

SkinnedMeshLoader::SkinnedMeshLoader()
{
}

SkinnedMeshLoader::SkinnedMeshLoader(const SkinnedMeshLoader &)
{
}

SkinnedMeshLoader &SkinnedMeshLoader::operator=(const SkinnedMeshLoader &)
{
    return (*this);
}

SkinnedMeshLoader::~SkinnedMeshLoader()
{
}

bool SkinnedMeshLoader::read_vec4_floats(const JsonValue *value, float out[4], float default_value)
{
    for (int i = 0; i < 4; i++)
        out[i] = default_value;
    if (value == nullptr || !value->is_array())
        return (false);
    for (size_t i = 0; i < value->array_elements().size() && i < 4; i++)
        out[i] = static_cast<float>(value->array_elements()[i].as_number());
    return (true);
}

bool SkinnedMeshLoader::read_bones(const JsonValue &bones_field, SkinnedAsset *out_asset)
{
    for (const JsonValue &bone_json : bones_field.array_elements())
    {
        Bone bone;
        bone.set_name(bone_json.find("name") != nullptr
            ? bone_json.find("name")->as_string() : std::string());
        const JsonValue *parent_field = bone_json.find("parent");
        bone.set_parent_index((parent_field != nullptr)
            ? static_cast<int32_t>(parent_field->as_number(-1)) : Bone::no_parent_index());
        bone.set_bind_local_position(bone_json.find("position") != nullptr
            ? bone_json.find("position")->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));
        bone.set_bind_local_rotation(bone_json.find("rotation") != nullptr
            ? bone_json.find("rotation")->as_vec3(vec3(0.0f, 0.0f, 0.0f)) : vec3(0.0f, 0.0f, 0.0f));
        out_asset->skeleton().bones().push_back(bone);
    }
    out_asset->skeleton().compute_bind_pose();
    return (true);
}

bool SkinnedMeshLoader::load(const char *path, SkinnedAsset *out_asset)
{
    JsonValue root;
    if (!JsonParser::load_file(path, &root))
    {
        std::fprintf(stderr, "load_skinned_asset: could not parse \"%s\" as JSON\n", path);
        return (false);
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
        return (false);
    }

    SkinnedAsset asset;
    read_bones(*bones_field, &asset);

    for (const JsonValue &vertex_json : vertices_field->array_elements())
        read_vertex(vertex_json, &asset);

    for (const JsonValue &index_json : indices_field->array_elements())
        asset.mesh().indices().push_back(static_cast<uint32_t>(index_json.as_number()));

    SubMesh submesh;
    submesh.set_index_offset(0);
    submesh.set_index_count(static_cast<uint32_t>(asset.mesh().indices().size()));
    submesh.set_material_index(-1); // untextured default material — see obj_loader.cpp's own convention
    asset.mesh().submeshes().push_back(submesh);

    read_animation(root, &asset);

    std::fprintf(stderr,
        "Renderer: loaded skinned asset \"%s\": %zu bone(s), %zu vertices, %zu indices, "
        "%zu animation track(s)\n",
        path, asset.skeleton().bones().size(), asset.mesh().vertices().size(),
        asset.mesh().indices().size(), asset.clip().tracks().size());

    *out_asset = std::move(asset);
    return (true);
}

} // namespace vre
