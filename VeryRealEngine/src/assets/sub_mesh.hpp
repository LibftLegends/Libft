/**
 * @file sub_mesh.hpp
 * @brief One contiguous run of indices in a mesh's index buffer that shares
 * a single material — i.e. what `usemtl` splits an OBJ file into.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class SubMesh
{
  public:
	SubMesh();
	SubMesh(const SubMesh &other);
	SubMesh &operator=(const SubMesh &other);
	~SubMesh();

	uint32_t index_offset() const;
	void set_index_offset(uint32_t value);

	uint32_t index_count() const;
	void set_index_count(uint32_t value);

	/// -1 => no material (untextured default).
	int32_t material_index() const;
	void set_material_index(int32_t value);

  private:
	uint32_t _index_offset;
	uint32_t _index_count;
	int32_t _material_index;
};

} // namespace vre
