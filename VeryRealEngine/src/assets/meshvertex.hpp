/**
 * @file meshvertex.hpp
 * @brief One vertex: position, normal, UV, plus up-to-4-bone GPU
 * linear-blend skinning data, matching the layout the graphics pipeline
 * expects (see mesh.vert's skinning computation).
 *
 * `bone_indices`/`bone_weights` exist for every vertex, not just skinned
 * meshes: an ordinary static mesh (anything loaded via ObjLoader) is simply
 * bound entirely to bone slot 0 with weight 1.0, and
 * Renderer::GlobalUbo::bone_matrices[0] is always the identity matrix — so
 * `skin_matrix` in mesh.vert reduces to the identity for every vertex that
 * doesn't actually belong to an animated skeleton, and static geometry is
 * unaffected. This is what lets one shader/pipeline serve both static and
 * skinned meshes rather than needing two.
 *
 * Every field has the same (private) access control and there are no
 * virtual functions or base classes, so this remains a standard-layout
 * type — required for `offsetof(MeshVertex, ...)` (see renderer.cpp's
 * vertex attribute descriptions) to stay well-defined.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class MeshVertex
{
  public:
	MeshVertex();
	MeshVertex(const MeshVertex &other);
	MeshVertex &operator=(const MeshVertex &other);
	~MeshVertex();

	float position(size_t index) const;
	void set_position(size_t index, float value);
	/** @brief Bulk-copies 3 floats into position,
		e.g. from a loader's own local buffer. */
	void set_position(const float value[3]);

	float normal(size_t index) const;
	void set_normal(size_t index, float value);
	void set_normal(const float value[3]);

	float uv(size_t index) const;
	void set_uv(size_t index, float value);
	void set_uv(const float value[2]);

	/** Index into GlobalUbo::bone_matrices. */
	float bone_index(size_t index) const;
	void set_bone_index(size_t index, float value);

	/** Must sum to 1.0 per vertex. */
	float bone_weight(size_t index) const;
	void set_bone_weight(size_t index, float value);

	// Byte offsets of each field within the struct, for Vulkan's
	// VkVertexInputAttributeDescription::offset — offsetof() itself
	// needs the actual (private) field name and so can only be
	// evaluated from inside this class, not by renderer.cpp directly.
	static size_t position_offset();
	static size_t normal_offset();
	static size_t uv_offset();
	static size_t bone_indices_offset();
	static size_t bone_weights_offset();

  private:
	float _position[3];
	float _normal[3];
	float _uv[2];
	float _bone_indices[4];
	float _bone_weights[4];
};

} // namespace vre
