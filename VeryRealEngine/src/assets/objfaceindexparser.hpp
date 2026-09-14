/**
 * @file objfaceindexparser.hpp
 * @brief Parses one OBJ face-index token ("v/vt/vn", "v//vn", "v/vt", or
 * bare "v") into 0-based position/uv/normal indices.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class ObjFaceIndexParser
{
  public:
	ObjFaceIndexParser();
	ObjFaceIndexParser(const ObjFaceIndexParser &other);
	ObjFaceIndexParser &operator=(const ObjFaceIndexParser &other);
	~ObjFaceIndexParser();

	/**
		* @brief Parses `token`.
		*
		* Indices in OBJ are 1-based and may be negative (relative to the
		* end of the list so far); this resolves both forms to 0-based
		* absolute indices, or -1 for a part the token left blank.
		* @param token One whitespace-separated face-index token.
		* @param position_count Positions parsed so far (for resolving a
		* negative index).
		* @param uv_count Same, for uvs.
		* @param normal_count Same, for normals.
		* @param out_position Receives the resolved position index.
		* @param out_uv Receives the resolved uv index.
		* @param out_normal Receives the resolved normal index.
		*/
	static void parse(const std::string &token, size_t position_count,
		size_t uv_count, size_t normal_count, int64_t *out_position,
		int64_t *out_uv, int64_t *out_normal);

  private:
	static int64_t resolve_index(const std::string &part, size_t count);
};

} // namespace vre
