/**
 * @file tga_loader.hpp
 * @brief Minimal uncompressed-TGA loader.
 *
 * TGA (specifically the uncompressed 24/32-bit true-color variant) is
 * simple enough to decode by hand, so textures don't need a third-party
 * image library — same "no non-system library" reasoning as the OBJ
 * loader (see obj_loader.hpp).
 */
#pragma once

#include <cstdint>
#include <vector>

namespace vre
{

/// Decoded image pixel data.
struct ImageData
{
    std::vector<uint8_t> pixels; ///< Tightly packed RGBA8, row 0 = top of image.
    uint32_t width = 0;
    uint32_t height = 0;
};

/**
 * @brief Decodes an uncompressed 24/32-bit true-color TGA file.
 * @param path Filesystem path to the .tga file.
 * @param out_image Receives the decoded pixel data.
 * @return true on success.
 */
bool load_tga(const char *path, ImageData *out_image);

} // namespace vre
