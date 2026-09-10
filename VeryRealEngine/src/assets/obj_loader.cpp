#include "obj_loader.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace vre
{

static std::string directory_of(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return "";
    return path.substr(0, slash + 1);
}

// Parses a .mtl file, appending each `newmtl` block found as a MaterialData.
static bool load_mtl(const std::string &path, const std::string &base_directory,
    std::vector<MaterialData> *out_materials)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::fprintf(stderr, "obj_loader: failed to open mtl file \"%s\"\n", path.c_str());
        return false;
    }

    MaterialData *current = nullptr;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string keyword;
        stream >> keyword;

        if (keyword == "newmtl")
        {
            std::string name;
            stream >> name;
            out_materials->push_back(MaterialData{});
            current = &out_materials->back();
            current->name = name;
        }
        else if (keyword == "Kd" && current != nullptr)
        {
            stream >> current->diffuse_color[0] >> current->diffuse_color[1]
                >> current->diffuse_color[2];
        }
        else if (keyword == "map_Kd" && current != nullptr)
        {
            std::string texture_name;
            stream >> texture_name;
            current->diffuse_texture_path = base_directory + texture_name;
        }
    }
    return true;
}

// Parses "v/vt/vn", "v//vn", "v/vt", or bare "v" face-index tokens.
// Indices in OBJ are 1-based and may be negative (relative to the end of
// the list so far); this resolves both forms to 0-based absolute indices.
static void parse_face_index_token(const std::string &token,
    size_t position_count, size_t uv_count, size_t normal_count,
    int64_t *out_position, int64_t *out_uv, int64_t *out_normal)
{
    *out_position = -1;
    *out_uv = -1;
    *out_normal = -1;

    std::string parts[3];
    int part_index = 0;
    for (char c : token)
    {
        if (c == '/')
        {
            part_index++;
            if (part_index > 2)
                break;
        }
        else
        {
            parts[part_index] += c;
        }
    }

    auto resolve = [](const std::string &part, size_t count) -> int64_t
    {
        if (part.empty())
            return -1;
        int64_t value = std::stoll(part);
        if (value < 0)
            return static_cast<int64_t>(count) + value;
        return value - 1;
    };

    if (!parts[0].empty())
        *out_position = resolve(parts[0], position_count);
    if (!parts[1].empty())
        *out_uv = resolve(parts[1], uv_count);
    if (!parts[2].empty())
        *out_normal = resolve(parts[2], normal_count);
}

bool load_obj(const char *path, MeshData *out_mesh, std::vector<MaterialData> *out_materials)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::fprintf(stderr, "obj_loader: failed to open \"%s\"\n", path);
        return false;
    }

    std::string base_directory = directory_of(path);

    struct Vec3f { float v[3]; };
    struct Vec2f { float v[2]; };
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

    auto flush_submesh = [&](uint32_t index_end)
    {
        if (has_open_submesh && index_end > current_submesh_start)
        {
            SubMesh submesh;
            submesh.index_offset = current_submesh_start;
            submesh.index_count = index_end - current_submesh_start;
            submesh.material_index = current_material_index;
            out_mesh->submeshes.push_back(submesh);
        }
    };

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
                material_name_to_index[(*out_materials)[i].name] = static_cast<int32_t>(i);
        }
        else if (keyword == "usemtl")
        {
            std::string material_name;
            stream >> material_name;

            flush_submesh(static_cast<uint32_t>(out_mesh->indices.size()));

            auto it = material_name_to_index.find(material_name);
            current_material_index = (it != material_name_to_index.end()) ? it->second : -1;
            current_submesh_start = static_cast<uint32_t>(out_mesh->indices.size());
            has_open_submesh = true;
        }
        else if (keyword == "f")
        {
            if (!has_open_submesh)
            {
                current_submesh_start = static_cast<uint32_t>(out_mesh->indices.size());
                has_open_submesh = true;
            }

            std::vector<uint32_t> face_indices;
            std::string token;
            while (stream >> token)
            {
                int64_t position_index, uv_index, normal_index;
                parse_face_index_token(token, positions.size(), uvs.size(), normals.size(),
                    &position_index, &uv_index, &normal_index);

                auto key = std::make_tuple(position_index, uv_index, normal_index);
                auto cached = vertex_cache.find(key);
                if (cached != vertex_cache.end())
                {
                    face_indices.push_back(cached->second);
                    continue;
                }

                MeshVertex vertex{};
                if (position_index >= 0 && position_index < static_cast<int64_t>(positions.size()))
                    std::memcpy(vertex.position, positions[position_index].v, sizeof(vertex.position));
                if (normal_index >= 0 && normal_index < static_cast<int64_t>(normals.size()))
                    std::memcpy(vertex.normal, normals[normal_index].v, sizeof(vertex.normal));
                if (uv_index >= 0 && uv_index < static_cast<int64_t>(uvs.size()))
                    std::memcpy(vertex.uv, uvs[uv_index].v, sizeof(vertex.uv));

                uint32_t new_index = static_cast<uint32_t>(out_mesh->vertices.size());
                out_mesh->vertices.push_back(vertex);
                vertex_cache[key] = new_index;
                face_indices.push_back(new_index);
            }

            // Fan-triangulate n-gons (n >= 3): (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < face_indices.size(); i++)
            {
                out_mesh->indices.push_back(face_indices[0]);
                out_mesh->indices.push_back(face_indices[i]);
                out_mesh->indices.push_back(face_indices[i + 1]);
            }
        }
    }

    flush_submesh(static_cast<uint32_t>(out_mesh->indices.size()));

    if (out_mesh->submeshes.empty() && !out_mesh->indices.empty())
    {
        SubMesh submesh;
        submesh.index_offset = 0;
        submesh.index_count = static_cast<uint32_t>(out_mesh->indices.size());
        submesh.material_index = -1;
        out_mesh->submeshes.push_back(submesh);
    }

    return true;
}

} // namespace vre
