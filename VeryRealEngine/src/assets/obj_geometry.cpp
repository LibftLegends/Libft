/**
 * @file obj_geometry.cpp
 * @brief ObjLoader::load()'s body, split out of obj_loader.cpp purely to
 * keep each file under this project's 250-line cap — both files define
 * methods of the same ObjLoader class declared in obj_loader.hpp.
 */
#include "obj_loader.hpp"

namespace vre
{

namespace
{

struct Vec3f
{
        float v[3];
};

struct Vec2f
{
        float v[2];
};

} // namespace

bool ObjLoader::load(const char *path, MeshData *out_mesh, std::vector<MaterialData> *out_materials)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::fprintf(stderr, "obj_loader: failed to open \"%s\"\n", path);
        return (false);
    }

    std::string base_directory = directory_of(path);

    std::vector<Vec3f> positions;
    std::vector<Vec2f> uvs;
    std::vector<Vec3f> normals;

    // Maps a unique (position, uv, normal) index triple to its slot in the
    // final interleaved vertex buffer, so shared corners are deduplicated
    // the way an indexed draw call needs.
    std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> vertex_cache;

    std::map<std::string, int32_t> material_name_to_index;
    int32_t current_material_index = -1;
    uint32_t current_submesh_start = 0;
    bool has_open_submesh = false;

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (keyword == "v")
        {
            Vec3f p;
            stream >> p.v[0] >> p.v[1] >> p.v[2];
            positions.push_back(p);
        }
        else if (keyword == "vt")
        {
            Vec2f t;
            stream >> t.v[0] >> t.v[1];
            uvs.push_back(t);
        }
        else if (keyword == "vn")
        {
            Vec3f n;
            stream >> n.v[0] >> n.v[1] >> n.v[2];
            normals.push_back(n);
        }
        else if (keyword == "mtllib")
        {
            std::string mtl_file;
            stream >> mtl_file;
            load_mtl(base_directory + mtl_file, base_directory, out_materials);
            for (size_t i = 0; i < out_materials->size(); i++)
                material_name_to_index[(*out_materials)[i].name()] = static_cast<int32_t>(i);
        }
        else if (keyword == "usemtl")
        {
            std::string material_name;
            stream >> material_name;

            if (has_open_submesh && out_mesh->indices().size() > current_submesh_start)
            {
                SubMesh submesh;
                submesh.set_index_offset(current_submesh_start);
                submesh.set_index_count(static_cast<uint32_t>(out_mesh->indices().size()) - current_submesh_start);
                submesh.set_material_index(current_material_index);
                out_mesh->submeshes().push_back(submesh);
            }

            auto it = material_name_to_index.find(material_name);
            current_material_index = (it != material_name_to_index.end()) ? it->second : -1;
            current_submesh_start = static_cast<uint32_t>(out_mesh->indices().size());
            has_open_submesh = true;
        }
        else if (keyword == "f")
        {
            if (!has_open_submesh)
            {
                current_submesh_start = static_cast<uint32_t>(out_mesh->indices().size());
                has_open_submesh = true;
            }

            std::vector<uint32_t> face_indices;
            std::string token;
            while (stream >> token)
            {
                int64_t position_index;
                int64_t uv_index;
                int64_t normal_index;
                parse_face_index_token(token, positions.size(), uvs.size(), normals.size(),
                    &position_index, &uv_index, &normal_index);

                auto key = std::make_tuple(position_index, uv_index, normal_index);
                auto cached = vertex_cache.find(key);
                if (cached != vertex_cache.end())
                {
                    face_indices.push_back(cached->second);
                    continue;
                }

                MeshVertex vertex;
                if (position_index >= 0 && position_index < static_cast<int64_t>(positions.size()))
                    vertex.set_position(positions[position_index].v);
                if (normal_index >= 0 && normal_index < static_cast<int64_t>(normals.size()))
                    vertex.set_normal(normals[normal_index].v);
                if (uv_index >= 0 && uv_index < static_cast<int64_t>(uvs.size()))
                    vertex.set_uv(uvs[uv_index].v);

                uint32_t new_index = static_cast<uint32_t>(out_mesh->vertices().size());
                out_mesh->vertices().push_back(vertex);
                vertex_cache[key] = new_index;
                face_indices.push_back(new_index);
            }

            // Fan-triangulate n-gons (n >= 3): (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < face_indices.size(); i++)
            {
                out_mesh->indices().push_back(face_indices[0]);
                out_mesh->indices().push_back(face_indices[i]);
                out_mesh->indices().push_back(face_indices[i + 1]);
            }
        }
    }

    if (has_open_submesh && out_mesh->indices().size() > current_submesh_start)
    {
        SubMesh submesh;
        submesh.set_index_offset(current_submesh_start);
        submesh.set_index_count(static_cast<uint32_t>(out_mesh->indices().size()) - current_submesh_start);
        submesh.set_material_index(current_material_index);
        out_mesh->submeshes().push_back(submesh);
    }

    if (out_mesh->submeshes().empty() && !out_mesh->indices().empty())
    {
        SubMesh submesh;
        submesh.set_index_offset(0);
        submesh.set_index_count(static_cast<uint32_t>(out_mesh->indices().size()));
        submesh.set_material_index(-1);
        out_mesh->submeshes().push_back(submesh);
    }

    return (true);
}

} // namespace vre
