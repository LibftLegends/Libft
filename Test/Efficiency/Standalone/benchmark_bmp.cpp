#include "../../../Modules/BMP/bmp.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

template <typename Operation>
static int32_t benchmark_operation(const char *label, Operation operation)
{
    constexpr uint32_t ITERATIONS = 20U;
    uint32_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (operation(iteration, checksum) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n", label, elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    constexpr ft_size_t WIDTH = 641U;
    constexpr ft_size_t HEIGHT = 480U;
    std::vector<uint8_t> pixels(WIDTH * HEIGHT * 4U);
    ft_size_t index = 0U;
    while (index < pixels.size())
    {
        pixels[index] = static_cast<uint8_t>(index * 17U + index / 17U);
        index += 1U;
    }
    bmp_image image;
    if (image.initialize_rgb(pixels.data(), WIDTH, HEIGHT, FT_TRUE) != FT_ERR_SUCCESS)
        return (1);
    ft_size_t rgb_size = 0U;
    ft_size_t rgba_size = 0U;
    if (image.encoded_size(BMP_BIT_DEPTH_RGB24, &rgb_size) != FT_ERR_SUCCESS
        || image.encoded_size(BMP_BIT_DEPTH_RGBA32, &rgba_size) != FT_ERR_SUCCESS)
        return (1);
    std::vector<uint8_t> rgb(rgb_size);
    std::vector<uint8_t> rgba(rgba_size);
    const auto encode = [&image](std::vector<uint8_t> &output, uint16_t depth,
        uint32_t iteration, uint32_t &checksum)
    {
        ft_size_t written = 0U;
        const int32_t error = image.encode(output.data(), output.size(), &written, depth);
        if (error == FT_ERR_SUCCESS)
            checksum += output[written - 97U * (iteration + 1U)];
        return (error);
    };
    const auto decode = [](const std::vector<uint8_t> &input,
        uint32_t iteration, uint32_t &checksum)
    {
        bmp_image decoded;
        const int32_t error = decoded.initialize(input.data(), input.size());
        if (error == FT_ERR_SUCCESS)
            checksum += decoded.data()[decoded.pixel_size() - 97U * (iteration + 1U)];
        return (error);
    };
    if (benchmark_operation("encode RGB24", [&](uint32_t iteration, uint32_t &checksum)
        {
            return (encode(rgb, BMP_BIT_DEPTH_RGB24, iteration, checksum));
        })
        || benchmark_operation("encode RGBA32", [&](uint32_t iteration, uint32_t &checksum)
        {
            return (encode(rgba, BMP_BIT_DEPTH_RGBA32, iteration, checksum));
        })
        || benchmark_operation("decode RGB24", [&](uint32_t iteration, uint32_t &checksum)
        {
            return (decode(rgb, iteration, checksum));
        })
        || benchmark_operation("decode RGBA32", [&](uint32_t iteration, uint32_t &checksum)
        {
            return (decode(rgba, iteration, checksum));
        }))
        return (1);
    const auto import_image = [&pixels](uint32_t iteration, uint32_t &checksum)
    {
        bmp_image imported;
        const int32_t error = imported.initialize_rgb(pixels.data(), WIDTH, HEIGHT, FT_TRUE);
        if (error == FT_ERR_SUCCESS)
            checksum += imported.data()[imported.pixel_size() - 97U * (iteration + 1U)];
        return (error);
    };
    const auto resize = [&pixels](uint32_t iteration, uint32_t &checksum)
    {
        bmp_image resized;
        int32_t error = resized.initialize_rgb(pixels.data(), WIDTH, HEIGHT, FT_TRUE);
        if (error == FT_ERR_SUCCESS)
            error = resized.resize_nearest(1103U, 729U);
        if (error == FT_ERR_SUCCESS)
            checksum += resized.data()[resized.pixel_size() - 97U * (iteration + 1U)];
        return (error);
    };
    if (benchmark_operation("import RGBA", import_image)
        || benchmark_operation("import and resize", resize))
        return (1);
    return (0);
}
