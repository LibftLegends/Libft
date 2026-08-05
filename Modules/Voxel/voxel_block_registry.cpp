#include "voxel_block_registry.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Buffer/byte_buffer.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <pthread.h>

static const uint32_t TERRAIN_RUNTIME_BLOCK_ID_BASE =
    TERRAIN_GENERATOR_GOLD_ORE_BLOCK + 1U;
static const uint32_t TERRAIN_RUNTIME_BLOCK_CAPACITY = 256U;

struct terrain_runtime_block
{
    uint32_t block_id;
    terrain_block_metadata metadata;
    ft_string name;
    ft_string asset_paths[TERRAIN_BLOCK_ASSET_FACE_COUNT];
    ft_byte_buffer asset_data[TERRAIN_BLOCK_ASSET_FACE_COUNT];
};

static terrain_runtime_block *g_terrain_runtime_blocks[
    TERRAIN_RUNTIME_BLOCK_CAPACITY] = {};
static pthread_once_t g_terrain_runtime_mutex_once = PTHREAD_ONCE_INIT;
static pt_mutex *g_terrain_runtime_mutex = ft_nullptr;

static void terrain_runtime_initialize_mutex(void) noexcept
{
    void *memory_pointer;
    pt_mutex *mutex_pointer;

    memory_pointer = std::malloc(sizeof(pt_mutex));
    if (memory_pointer == ft_nullptr)
        return ;
    mutex_pointer = new (memory_pointer) pt_mutex();
    if (mutex_pointer->initialize() != FT_ERR_SUCCESS)
    {
        mutex_pointer->~pt_mutex();
        std::free(memory_pointer);
        return ;
    }
    g_terrain_runtime_mutex = mutex_pointer;
    return ;
}

static pt_mutex *terrain_runtime_get_mutex(void) noexcept
{
    if (pthread_once(&g_terrain_runtime_mutex_once,
            terrain_runtime_initialize_mutex) != 0)
        return (ft_nullptr);
    return (g_terrain_runtime_mutex);
}

static terrain_runtime_block *terrain_runtime_find_block(
    uint32_t block_id) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;
    terrain_runtime_block *block_pointer;

    if (block_id < TERRAIN_RUNTIME_BLOCK_ID_BASE)
        return (ft_nullptr);
    index = block_id - TERRAIN_RUNTIME_BLOCK_ID_BASE;
    if (index >= TERRAIN_RUNTIME_BLOCK_CAPACITY)
        return (ft_nullptr);
    mutex_pointer = terrain_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_nullptr);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    block_pointer = g_terrain_runtime_blocks[index];
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (block_pointer);
}

static void terrain_runtime_destroy_block(terrain_runtime_block *block_pointer,
    uint32_t initialized_asset_count) noexcept
{
    uint32_t index;

    if (block_pointer == ft_nullptr)
        return ;
    index = 0U;
    while (index < initialized_asset_count)
    {
        (void)block_pointer->asset_data[index].destroy();
        (void)block_pointer->asset_paths[index].destroy();
        index += 1U;
    }
    (void)block_pointer->name.destroy();
    block_pointer->~terrain_runtime_block();
    std::free(block_pointer);
    return ;
}

static int32_t terrain_runtime_load_asset(const char *path,
    ft_byte_buffer &asset_data) noexcept
{
    su_file *file_stream;
    int64_t file_size_long;
    ft_size_t file_size;
    ft_size_t read_count;
    void *temporary_data;
    int32_t error_code;

    if (path == ft_nullptr || path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    file_stream = su_fopen(path);
    if (file_stream == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    error_code = FT_ERR_SUCCESS;
    file_size_long = 0;
    if (su_fseek(file_stream, 0, SEEK_END) != 0)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
    {
        file_size_long = su_ftell(file_stream);
        if (file_size_long < 0)
            error_code = FT_ERR_IO;
    }
    if (error_code == FT_ERR_SUCCESS
        && su_fseek(file_stream, 0, SEEK_SET) != 0)
        error_code = FT_ERR_IO;
    file_size = 0U;
    temporary_data = ft_nullptr;
    if (error_code == FT_ERR_SUCCESS)
    {
        file_size = static_cast<ft_size_t>(file_size_long);
        if (file_size > 0U)
            temporary_data = std::malloc(file_size);
        if (file_size > 0U && temporary_data == ft_nullptr)
            error_code = FT_ERR_NO_MEMORY;
    }
    if (error_code == FT_ERR_SUCCESS && file_size > 0U)
    {
        read_count = su_fread(temporary_data, 1, file_size, file_stream);
        if (read_count != file_size)
            error_code = FT_ERR_IO;
    }
    if (su_fclose(file_stream) != 0 && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
        error_code = asset_data.initialize(file_size, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS && file_size > 0U)
        error_code = asset_data.append(temporary_data, file_size);
    std::free(temporary_data);
    return (error_code);
}

const terrain_block_metadata *terrain_runtime_find_block_metadata(
    uint32_t block_id) noexcept
{
    terrain_runtime_block *block_pointer;

    block_pointer = terrain_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (&block_pointer->metadata);
}

ft_bool terrain_runtime_block_is_known(uint32_t block_id) noexcept
{
    if (terrain_runtime_find_block(block_id) == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t terrain_register_block(const terrain_block_registration &registration,
    uint32_t *block_id_out) noexcept
{
    terrain_runtime_block *created_block;
    void *memory_pointer;
    pt_mutex *mutex_pointer;
    uint32_t existing_index;
    uint32_t index;
    uint32_t asset_index;
    int32_t error_code;

    if (block_id_out == ft_nullptr || registration.name == ft_nullptr
        || registration.name[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    asset_index = 0U;
    while (asset_index < TERRAIN_BLOCK_ASSET_FACE_COUNT)
    {
        if (registration.asset_paths[asset_index] == ft_nullptr
            || registration.asset_paths[asset_index][0] == '\0')
            return (FT_ERR_INVALID_ARGUMENT);
        asset_index += 1U;
    }
    mutex_pointer = terrain_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    existing_index = 0U;
    while (existing_index < TERRAIN_RUNTIME_BLOCK_CAPACITY)
    {
        if (g_terrain_runtime_blocks[existing_index] != ft_nullptr
            && std::strcmp(
                g_terrain_runtime_blocks[existing_index]->name.c_str(),
                registration.name) == 0)
        {
            (void)pt_mutex_unlock_if_not_null(mutex_pointer);
            return (FT_ERR_ALREADY_EXISTS);
        }
        existing_index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_RUNTIME_BLOCK_CAPACITY
        && g_terrain_runtime_blocks[index] != ft_nullptr)
        index += 1U;
    if (index == TERRAIN_RUNTIME_BLOCK_CAPACITY)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_OUT_OF_RANGE);
    }
    memory_pointer = std::malloc(sizeof(terrain_runtime_block));
    if (memory_pointer == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NO_MEMORY);
    }
    created_block = new (memory_pointer) terrain_runtime_block();
    created_block->block_id = TERRAIN_RUNTIME_BLOCK_ID_BASE + index;
    created_block->metadata = registration.metadata;
    error_code = created_block->name.initialize(registration.name);
    asset_index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && asset_index < TERRAIN_BLOCK_ASSET_FACE_COUNT)
    {
        error_code = created_block->asset_paths[asset_index].initialize(
            registration.asset_paths[asset_index]);
        if (error_code == FT_ERR_SUCCESS)
            error_code = terrain_runtime_load_asset(
                registration.asset_paths[asset_index],
                created_block->asset_data[asset_index]);
        asset_index += 1U;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        terrain_runtime_destroy_block(created_block, asset_index);
        return (error_code);
    }
    g_terrain_runtime_blocks[index] = created_block;
    *block_id_out = created_block->block_id;
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

const char *terrain_get_block_name(uint32_t block_id) noexcept
{
    terrain_runtime_block *block_pointer;

    block_pointer = terrain_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (block_pointer->name.c_str());
}

const char *terrain_get_block_asset_path(uint32_t block_id,
    terrain_block_asset_face face) noexcept
{
    terrain_runtime_block *block_pointer;

    if (face < TERRAIN_BLOCK_ASSET_FACE_TOP
        || face >= TERRAIN_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    block_pointer = terrain_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (block_pointer->asset_paths[face].c_str());
}

const uint8_t *terrain_get_block_asset_data(uint32_t block_id,
    terrain_block_asset_face face, ft_size_t *size_out) noexcept
{
    terrain_runtime_block *block_pointer;

    if (size_out == ft_nullptr)
        return (ft_nullptr);
    *size_out = 0U;
    if (face < TERRAIN_BLOCK_ASSET_FACE_TOP
        || face >= TERRAIN_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    block_pointer = terrain_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    *size_out = block_pointer->asset_data[face].size();
    return (block_pointer->asset_data[face].data());
}

#endif
