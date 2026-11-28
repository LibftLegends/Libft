#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Voxel/voxel_api.hpp"
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <thread>

using registry_clock = std::chrono::steady_clock;

static void measure_builtin_metadata()
{
    constexpr uint32_t LOOKUP_COUNT = 10000000U;
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto started = registry_clock::now();
    while (index < LOOKUP_COUNT)
    {
        const voxel_block_metadata &metadata =
            voxel_get_block_metadata(index % VOXEL_BUILTIN_BLOCK_COUNT);
        checksum += metadata.solid + metadata.emitted_light_level;
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        registry_clock::now() - started).count();
    std::printf("built-in metadata: %.2f ns/lookup, checksum %" FT_PRIu64 "\n",
        elapsed / LOOKUP_COUNT, checksum);
    return ;
}

static int32_t measure_builtin_names()
{
    constexpr uint32_t LOOKUP_COUNT = 500000U;
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto started = registry_clock::now();
    while (index < LOOKUP_COUNT)
    {
        const uint32_t block_id = index % VOXEL_BUILTIN_BLOCK_COUNT;
        uint32_t resolved_id;
        if (voxel_find_block_id_by_name(voxel_get_block_name(block_id), &resolved_id) != FT_ERR_SUCCESS
            || resolved_id != block_id)
            return (1);
        checksum += resolved_id;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        registry_clock::now() - started).count();
    std::printf("built-in names: %.2f ns/lookup, checksum %" FT_PRIu64 "\n",
        elapsed / LOOKUP_COUNT, checksum);
    return (0);
}

static int32_t measure_runtime_names(const char *asset_path)
{
    constexpr uint32_t LOOKUP_COUNT = 200000U;
    char names[VOXEL_RUNTIME_BLOCK_CAPACITY][40];
    uint32_t ids[VOXEL_RUNTIME_BLOCK_CAPACITY];
    voxel_block_registration registration = {};
    uint32_t face = 0U;
    while (face < VOXEL_BLOCK_ASSET_FACE_COUNT)
        registration.asset_paths[face++] = asset_path;
    uint32_t index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        const uint32_t name_index = VOXEL_RUNTIME_BLOCK_CAPACITY - index - 1U;
        std::snprintf(names[index], sizeof(names[index]), "benchmark:block_%" PRIu32, name_index);
        registration.name = names[index];
        if (voxel_register_block(registration, &ids[index]) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint64_t checksum = 0U;
    index = 0U;
    const auto started = registry_clock::now();
    while (index < LOOKUP_COUNT)
    {
        const uint32_t name_index = (index * 73U) % VOXEL_RUNTIME_BLOCK_CAPACITY;
        uint32_t resolved_id;
        if (voxel_find_block_id_by_name(names[name_index], &resolved_id) != FT_ERR_SUCCESS
            || resolved_id != ids[name_index]
            || voxel_find_block_id_by_name("benchmark:missing", &resolved_id) != FT_ERR_NOT_FOUND)
            return (1);
        checksum += resolved_id;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        registry_clock::now() - started).count();
    index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        if (voxel_unregister_block(ids[index]) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    std::printf("runtime names: %.2f ns/hit-miss pair, checksum %" FT_PRIu64 "\n",
        elapsed / LOOKUP_COUNT, checksum);
    return (0);
}

static int32_t measure_runtime_lookups_during_registration(const char *asset_path)
{
    constexpr uint32_t REGISTRATION_COUNT = 256U;
    voxel_block_registration registration = {};
    registration.name = "benchmark:anchor";
    uint32_t face_index = 0U;
    while (face_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        registration.asset_paths[face_index] = asset_path;
        face_index += 1U;
    }
    uint32_t anchor_id;
    if (voxel_register_block(registration, &anchor_id) != FT_ERR_SUCCESS)
        return (1);

    registration.name = "benchmark:transient";
    std::atomic<ft_bool> start_writer{FT_FALSE};
    std::atomic<ft_bool> writer_done{FT_FALSE};
    int32_t writer_error = FT_ERR_SUCCESS;
    std::thread writer([&]()
    {
        while (start_writer.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        uint32_t index = 0U;
        while (index < REGISTRATION_COUNT && writer_error == FT_ERR_SUCCESS)
        {
            uint32_t transient_id;
            writer_error = voxel_register_block(registration, &transient_id);
            if (writer_error == FT_ERR_SUCCESS)
                writer_error = voxel_unregister_block(transient_id);
            index += 1U;
        }
        writer_done.store(FT_TRUE, std::memory_order_release);
        return ;
    });
    uint64_t lookups = 0U;
    uint64_t hits = 0U;
    const auto started = registry_clock::now();
    start_writer.store(FT_TRUE, std::memory_order_release);
    while (writer_done.load(std::memory_order_acquire) == FT_FALSE)
    {
        hits += voxel_block_is_known(anchor_id);
        lookups += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        registry_clock::now() - started).count();
    writer.join();
    if (voxel_unregister_block(anchor_id) != FT_ERR_SUCCESS
        || writer_error != FT_ERR_SUCCESS || lookups == 0U || hits != lookups)
        return (1);
    std::printf("runtime lookups during registration: %.2f ns/lookup, %" FT_PRIu64 " hits\n",
        elapsed / static_cast<double>(lookups), hits);
    return (0);
}

static int32_t measure_asset_registration(const char *asset_path)
{
    constexpr uint32_t REGISTRATION_COUNT = 16U;
    voxel_block_registration registration = {};
    registration.name = "benchmark:asset";
    uint32_t face_index = 0U;
    while (face_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        registration.asset_paths[face_index] = asset_path;
        face_index += 1U;
    }
    double elapsed = 0.0;
    uint64_t checksum = 0U;
    ft_size_t asset_size = 0U;
    uint32_t index = 0U;
    while (index < REGISTRATION_COUNT)
    {
        uint32_t block_id;
        const auto started = registry_clock::now();
        if (voxel_register_block(registration, &block_id) != FT_ERR_SUCCESS)
            return (1);
        elapsed += std::chrono::duration<double, std::nano>(
            registry_clock::now() - started).count();
        face_index = 0U;
        while (face_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
        {
            const uint8_t *bytes = voxel_get_block_asset_data(block_id,
                static_cast<voxel_block_asset_face>(face_index), &asset_size);
            if (bytes == ft_nullptr || asset_size == 0U)
                return (1);
            checksum = (checksum * 31U + bytes[0]) * 31U + bytes[asset_size - 1U];
            face_index += 1U;
        }
        if (voxel_unregister_block(block_id) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    std::printf("asset registration: %.2f ns/block, %" FT_PRIu64
        " bytes/face, checksum %" FT_PRIu64 "\n",
        elapsed / REGISTRATION_COUNT, asset_size, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    measure_builtin_metadata();
    if (measure_builtin_names() != 0)
        return (1);
    if (argument_count < 2 || argument_count > 3)
    {
        std::fputs("Pass a small readable texture asset path and optionally a large one.\n",
            stderr);
        return (1);
    }
    if (measure_runtime_names(arguments[1]) != 0
        || measure_runtime_lookups_during_registration(arguments[1]) != 0)
        return (1);
    if (argument_count == 3)
        return (measure_asset_registration(arguments[2]));
    return (0);
}
