/**
 * @file skinnedmeshvertexreader.hpp
 * @brief Reads one skinned-mesh JSON vertex object into a MeshVertex.
 */
#pragma once

#include "../vre.hpp"
#include "jsonvalue.hpp"
#include "meshvertex.hpp"

namespace vre
{
class SkinnedMeshVertexReader
{
  public:
	SkinnedMeshVertexReader();
	SkinnedMeshVertexReader(const SkinnedMeshVertexReader &other);
	SkinnedMeshVertexReader &operator=(
		const SkinnedMeshVertexReader &other);
	~SkinnedMeshVertexReader();

	/**
		* @brief Reads `vertex_json`'s "position"/"normal"/"uv"/
		* "bone_indices"/"bone_weights" fields into a MeshVertex.
		*
		* Every local bone index is offset by +1 in the result: global
		* bone slot 0 is permanently the identity matrix (see
		* meshvertex.hpp's doc comment and Renderer::update_global_ubo),
		* so this asset's own bones — indexed 0-based here and in its
		* animation tracks — occupy global slots [1, bone_count].
		*/
	static MeshVertex read(const JsonValue &vertex_json);

  private:
	/// @return `value` parsed as up to 4 floats, or all `default_value`
	/// if malformed.
	static bool read_vec4_floats(const JsonValue *value, float out[4],
		float default_value);
};

} // namespace vre
