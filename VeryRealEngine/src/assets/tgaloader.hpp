/**
 * @file tgaloader.hpp
 * @brief Minimal uncompressed-TGA loader.
 *
 * TGA (specifically the uncompressed 24/32-bit true-color variant) is
 * simple enough to decode by hand, so textures don't need a third-party
 * image library — same "no non-system library" reasoning as the OBJ
 * loader (see objloader.hpp).
 */
#pragma once

#include "../vre.hpp"
#include "imagedata.hpp"

namespace vre
{
class TgaLoader
{
  public:
	TgaLoader();
	TgaLoader(const TgaLoader &other);
	TgaLoader &operator=(const TgaLoader &other);
	~TgaLoader();

	/**
		* @brief Decodes an uncompressed 24/32-bit true-color TGA file.
		* @param path Filesystem path to the .tga file.
		* @param out_image Receives the decoded pixel data.
		* @return true on success.
		*/
	static bool load(const char *path, ImageData *out_image);
};

} // namespace vre
