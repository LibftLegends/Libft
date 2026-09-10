// Minimal uncompressed-TGA loader.
//
// TGA (specifically the uncompressed 24/32-bit true-color variant) is
// simple enough to decode by hand, so textures don't need a third-party
// image library — same "no non-system library" reasoning as the OBJ
// loader (see obj_loader.hpp).
#pragma once

#include <cstdint>
#include <vector>

namespace vre
{

struct ImageData
{
    std::vector<uint8_t> pixels; // tightly packed RGBA8, row 0 = top of image
    uint32_t width = 0;
    uint32_t height = 0;
};

bool load_tga(const char *path, ImageData *out_image);

} // namespace vre
