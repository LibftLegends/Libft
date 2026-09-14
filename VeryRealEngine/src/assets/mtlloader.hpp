/**
 * @file mtlloader.hpp
 * @brief Minimal Wavefront .MTL loader: diffuse color/texture plus the
 * Pr/Pm roughness/metallic PBR extension (the same convention Blender's
 * OBJ exporter and several other tools use).
 */
#pragma once

#include "../vre.hpp"
#include "materialdata.hpp"

namespace vre
{
class MtlLoader
{
  public:
	MtlLoader();
	MtlLoader(const MtlLoader &other);
	MtlLoader &operator=(const MtlLoader &other);
	~MtlLoader();

	/**
		* @brief Parses `path`, appending each `newmtl` block found as a
		* MaterialData to `out_materials`.
		* @param path Filesystem path to the .mtl file.
		* @param base_directory Directory `map_Kd` texture paths are
		* resolved relative to (typically the .obj's own directory).
		* @param out_materials Parsed materials are appended here.
		* @return true on success.
		*/
	static bool load(const std::string &path,
		const std::string &base_directory,
		std::vector<MaterialData> *out_materials);
};

} // namespace vre
