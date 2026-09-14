/**
 * @file obj_loader.hpp
 * @brief Minimal Wavefront .OBJ (+ .MTL) loader.
 *
 * Written from scratch for VeryRealEngine — no third-party OBJ library —
 * consistent with the subject's "no non-system library" rule. Supports the
 * subset the engine actually needs: positions/normals/uvs, n-gon faces
 * (triangulated as a fan), `usemtl` material grouping into submeshes, and
 * `mtllib` diffuse color / diffuse texture map.
 */
#pragma once

#include "../vre.hpp"
#include "material_data.hpp"
#include "mesh_data.hpp"

namespace vre
{

class ObjLoader
{
  public:
	ObjLoader();
	ObjLoader(const ObjLoader &other);
	ObjLoader &operator=(const ObjLoader &other);
	~ObjLoader();

	/**
		* @brief Loads `path` into `out_mesh` and appends any materials referenced
		* via `mtllib` to `out_materials`.
		*
		* Paths inside `out_materials` are resolved relative to the .obj's
		* directory, ready to hand to the texture loader.
		* @param path Filesystem path to the .obj file.
		* @param out_mesh Receives the parsed geometry.
		* @param out_materials Materials referenced by the file are appended here.
		* @return (true on success); false (leaving *out_mesh partially filled) on parse/IO failure.
		*/
	static bool load(const char *path, MeshData *out_mesh,
		std::vector<MaterialData> *out_materials);

  private:
	static std::string directory_of(const std::string &path);
	/** Parses a .mtl file,
		appending each `newmtl` block found as a MaterialData. */
	static bool load_mtl(const std::string &path,
		const std::string &base_directory,
		std::vector<MaterialData> *out_materials);
	/**
		* @brief Parses "v/vt/vn", "v//vn", "v/vt",
			or bare "v" face-index tokens.
		*
		* Indices in OBJ are 1-based and may be negative (relative to the
		* end of the list so far); this resolves both forms to 0-based
		* absolute indices.
		*/
	static void parse_face_index_token(const std::string &token,
		size_t position_count, size_t uv_count, size_t normal_count,
		int64_t *out_position, int64_t *out_uv, int64_t *out_normal);
	static int64_t resolve_index(const std::string &part, size_t count);
};

} // namespace vre
