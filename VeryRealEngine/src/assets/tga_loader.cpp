#include "tga_loader.hpp"

#include <cstdio>
#include <fstream>

namespace vre
{

#pragma pack(push, 1)
struct TgaHeader
{
    uint8_t id_length;
    uint8_t color_map_type;
    uint8_t image_type;
    uint16_t color_map_first_entry;
    uint16_t color_map_length;
    uint8_t color_map_entry_size;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t pixel_depth;
    uint8_t image_descriptor;
};
#pragma pack(pop)

bool load_tga(const char *path, ImageData *out_image)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        std::fprintf(stderr, "tga_loader: failed to open \"%s\"\n", path);
        return false;
    }

    TgaHeader header;
    file.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!file)
    {
        std::fprintf(stderr, "tga_loader: \"%s\" is too short for a TGA header\n", path);
        return false;
    }

    // Only the uncompressed true-color path (image type 2) is supported —
    // that covers every TGA this engine writes/ships itself. Paletted (1)
    // and RLE-compressed (10) variants are out of scope for this loader.
    if (header.image_type != 2)
    {
        std::fprintf(stderr,
            "tga_loader: \"%s\" uses unsupported image type %u (only uncompressed "
            "true-color TGA, type 2, is supported)\n", path, header.image_type);
        return false;
    }
    if (header.pixel_depth != 24 && header.pixel_depth != 32)
    {
        std::fprintf(stderr,
            "tga_loader: \"%s\" has unsupported pixel depth %u (need 24 or 32)\n",
            path, header.pixel_depth);
        return false;
    }

    file.seekg(header.id_length, std::ios::cur);
    if (header.color_map_type != 0)
    {
        uint32_t color_map_bytes =
            header.color_map_length * (header.color_map_entry_size / 8);
        file.seekg(color_map_bytes, std::ios::cur);
    }

    uint32_t width = header.width;
    uint32_t height = header.height;
    uint32_t bytes_per_pixel = header.pixel_depth / 8;

    std::vector<uint8_t> raw(static_cast<size_t>(width) * height * bytes_per_pixel);
    file.read(reinterpret_cast<char *>(raw.data()), static_cast<std::streamsize>(raw.size()));
    if (!file)
    {
        std::fprintf(stderr, "tga_loader: \"%s\" is truncated\n", path);
        return false;
    }

    // Bit 5 of the image descriptor: 1 => rows are stored top-to-bottom,
    // 0 (the TGA default) => bottom-to-top. Normalize to top-to-bottom RGBA8.
    bool top_to_bottom = (header.image_descriptor & 0x20) != 0;

    out_image->width = width;
    out_image->height = height;
    out_image->pixels.resize(static_cast<size_t>(width) * height * 4);

    for (uint32_t y = 0; y < height; y++)
    {
        uint32_t source_row = top_to_bottom ? y : (height - 1 - y);
        const uint8_t *source = raw.data() + static_cast<size_t>(source_row) * width * bytes_per_pixel;
        uint8_t *dest = out_image->pixels.data() + static_cast<size_t>(y) * width * 4;

        for (uint32_t x = 0; x < width; x++)
        {
            // TGA true-color pixels are stored BGR(A).
            uint8_t b = source[x * bytes_per_pixel + 0];
            uint8_t g = source[x * bytes_per_pixel + 1];
            uint8_t r = source[x * bytes_per_pixel + 2];
            uint8_t a = (bytes_per_pixel == 4) ? source[x * bytes_per_pixel + 3] : 255;

            dest[x * 4 + 0] = r;
            dest[x * 4 + 1] = g;
            dest[x * 4 + 2] = b;
            dest[x * 4 + 3] = a;
        }
    }

    return true;
}

} // namespace vre
