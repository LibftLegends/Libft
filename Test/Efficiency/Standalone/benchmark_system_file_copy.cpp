#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include "../../../Modules/System_utils/system_utils.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

static int32_t measure_copy(ft_size_t bytes, uint32_t iterations)
{
    ft_string source;
    ft_string destination;
    if (source.initialize() != FT_ERR_SUCCESS || destination.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_copy_source", "bin", &source) != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_copy_destination", "bin", &destination) != FT_ERR_SUCCESS)
        return (1);

    const std::vector<char> payload(FT_UINT64_TO_NATIVE_SIZE_CAST(bytes), 'a');
    int32_t result = file_write_all(source.c_str(), payload.data(), bytes);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (result == FT_ERR_SUCCESS && iteration < iterations)
    {
        if (su_copy_file(source.c_str(), destination.c_str()) != 0)
            result = FT_ERR_IO;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    ft_size_t copied = 0U;
    if (result == FT_ERR_SUCCESS)
        result = file_get_size(destination.c_str(), &copied);
    (void)file_delete(destination.c_str());
    (void)file_delete(source.c_str());
    if (result != FT_ERR_SUCCESS || copied != bytes)
        return (1);

    std::printf("file copy %" PRIu64 " bytes %.2f us/op\n", copied,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    return (0);
}

int32_t main()
{
    if (measure_copy(4096U, 96U) != 0)
        return (1);
    return (measure_copy(4U * 1024U * 1024U, 24U));
}
