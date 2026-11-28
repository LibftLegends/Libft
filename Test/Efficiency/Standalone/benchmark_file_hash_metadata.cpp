#include "../../../Modules/Encryption/encryption.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

static int32_t measure_hashes(const char *path, const std::vector<char> &payload)
{
    constexpr uint32_t ITERATIONS = 8;
    uint8_t expected_sha1[20];
    uint8_t expected_sha256[32];
    sha1_hash(payload.data(), payload.size(), expected_sha1);
    sha256_hash(payload.data(), payload.size(), expected_sha256);
    uint32_t iteration = 0;
    const auto sha1_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        uint8_t digest[20];
        if (file_hash_sha1(path, digest) != FT_ERR_SUCCESS
            || ft_memcmp(digest, expected_sha1, sizeof(digest)) != 0)
            return (1);
        ++iteration;
    }
    const auto sha1_elapsed = std::chrono::steady_clock::now() - sha1_start;
    iteration = 0;
    const auto sha256_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        uint8_t digest[32];
        if (file_hash_sha256(path, digest) != FT_ERR_SUCCESS
            || ft_memcmp(digest, expected_sha256, sizeof(digest)) != 0)
            return (1);
        ++iteration;
    }
    const auto sha256_elapsed = std::chrono::steady_clock::now() - sha256_start;
    std::printf("file_hash_sha1 bytes=%zu ms/op=%.3f\n", payload.size(),
        std::chrono::duration<double, std::milli>(sha1_elapsed).count() / ITERATIONS);
    std::printf("file_hash_sha256 bytes=%zu ms/op=%.3f\n", payload.size(),
        std::chrono::duration<double, std::milli>(sha256_elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_metadata(const char *left, const char *right)
{
    constexpr uint32_t ITERATIONS = 5000;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        uint32_t difference = FILE_METADATA_DIFF_NONE;
        if (file_metadata_diff(left, right, &difference) != FT_ERR_SUCCESS
            || difference != FILE_METADATA_DIFF_SIZE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("file_metadata_diff us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 8U * 1024U * 1024U;
    ft_string large_path;
    ft_string small_path;
    if (filesystem_temp_path("libft_hash_large", "bin", &large_path) != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_hash_small", "bin", &small_path) != FT_ERR_SUCCESS)
        return (1);
    const std::vector<char> payload(PAYLOAD_SIZE, 'a');
    int32_t result = 1;
    if (file_write_all(large_path.c_str(), payload.data(), payload.size()) == FT_ERR_SUCCESS
        && file_write_all(small_path.c_str(), "x", 1) == FT_ERR_SUCCESS
        && measure_hashes(large_path.c_str(), payload) == 0)
        result = measure_metadata(large_path.c_str(), small_path.c_str());
    const int32_t large_cleanup = file_delete(large_path.c_str());
    const int32_t small_cleanup = file_delete(small_path.c_str());
    if (large_cleanup != FT_ERR_SUCCESS || small_cleanup != FT_ERR_SUCCESS)
        return (1);
    return (result);
}
