/**
 * @file geometrybuilder.hpp
 * @brief Incrementally builds a MeshData (and any materials it
 * references) from an OBJ file's lines, one keyword at a time.
 *
 * Fan-triangulates n-gons (n >= 3) as (0,1,2), (0,2,3), (0,3,4), ... and
 * deduplicates shared (position, uv, normal) corners into one indexed
 * vertex buffer.
 */
#pragma once

#include "../vre.hpp"
#include "../materials/data.hpp"
#include "../mesh/data.hpp"
#include "../mesh/vertex.hpp"
#include "../materials/mtlloader.hpp"
#include "faceindexparser.hpp"
#include "../mesh/submesh.hpp"

namespace vre
{
class ObjGeometryBuilder
{
  public:
	ObjGeometryBuilder();
	~ObjGeometryBuilder();

	/**
		* @brief Constructs a builder that writes into `out_mesh`/
		* `out_materials`, resolving `mtllib`-referenced paths against
		* `base_directory`.
		*/
	ObjGeometryBuilder(const std::string &base_directory, MeshData *out_mesh,
		std::vector<MaterialData> *out_materials);

	/// Parses one already-read line, dispatching on its leading keyword
	/// ("v", "vt", "vn", "mtllib", "usemtl", "f"; anything else is ignored).
	void parse_line(const std::string &line);
	/// Closes the final open submesh (or synthesizes a default one).
	/// Call once, after the last parse_line().
	void finish();

  private:
	struct Vec3f
	{
		float v[3];
	};

	struct Vec2f
	{
		float v[2];
	};

	// Owns pointers into the caller's out_mesh/out_materials — copying
	// this object would silently write two builders into the same
	// buffers, so, like Renderer/Scene, it gets the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator (this
	// project avoids `= delete`).
	ObjGeometryBuilder(const ObjGeometryBuilder &other);
	ObjGeometryBuilder &operator=(const ObjGeometryBuilder &other);

	void parse_position(std::istringstream *stream);
	void parse_uv(std::istringstream *stream);
	void parse_normal(std::istringstream *stream);
	void parse_mtllib(std::istringstream *stream);
	void parse_usemtl(std::istringstream *stream);
	void parse_face(std::istringstream *stream);
	void close_current_submesh();

	std::string _base_directory;
	MeshData *_out_mesh;
	std::vector<MaterialData> *_out_materials;

	std::vector<Vec3f> _positions;
	std::vector<Vec2f> _uvs;
	std::vector<Vec3f> _normals;
	std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> _vertex_cache;
	std::map<std::string, int32_t> _material_name_to_index;
	int32_t _current_material_index;
	uint32_t _current_submesh_start;
	bool _has_open_submesh;
};

} // namespace vre
