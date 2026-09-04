#include "voxel_block_registry.hpp"
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../File/file_utils.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <pthread.h>

static const uint32_t VOXEL_RUNTIME_BLOCK_ID_BASE =
    static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT);

voxel_runtime_block::voxel_runtime_block() noexcept
    : block_id(0U), metadata(), name(), asset_paths(), asset_data()
{
    return ;
}

voxel_runtime_block::~voxel_runtime_block() noexcept
{
    uint32_t index;

    index = 0U;
    while (index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        (void)this->asset_data[index].destroy();
        (void)this->asset_paths[index].destroy();
        index += 1U;
    }
    (void)this->name.destroy();
    return ;
}

static ft_sharedptr<voxel_runtime_block> g_voxel_runtime_blocks[
    VOXEL_RUNTIME_BLOCK_CAPACITY] = {};

static const char *const VOXEL_BUILTIN_BLOCK_NAMES[] =
{
    "voxel:air", "voxel:grass", "voxel:dirt",
    "voxel:stone", "voxel:shrub", "voxel:oak_log",
    "voxel:oak_leaves", "voxel:cactus", "voxel:water",
    "voxel:bedrock", "voxel:sand", "voxel:snow",
    "voxel:permafrost", "voxel:canyon_rock", "voxel:slate",
    "voxel:moss_rock", "voxel:coal_ore", "voxel:iron_ore",
    "voxel:gold_ore", "voxel:granite", "voxel:andesite",
    "voxel:diorite", "voxel:gravel", "voxel:clay",
    "voxel:obsidian", "voxel:mossy_stone",
    "voxel:cracked_stone", "voxel:limestone", "voxel:basalt",
    "voxel:diamond_ore", "voxel:emerald_ore",
    "voxel:copper_ore", "voxel:pine_log", "voxel:pine_leaves",
    "voxel:birch_log", "voxel:birch_leaves", "voxel:ice",
    "voxel:packed_ice", "voxel:red_flower",
    "voxel:yellow_flower", "voxel:tall_grass", "voxel:fern",
    "voxel:dead_bush", "voxel:red_mushroom",
    "voxel:brown_mushroom", "voxel:mushroom_stem",
    "voxel:lily_pad", "voxel:seagrass", "voxel:coarse_dirt",
    "voxel:podzol", "voxel:mud", "voxel:frozen_stone",
    "voxel:chalk", "voxel:red_sand", "voxel:terracotta",
    "voxel:salt", "voxel:volcanic_rock", "voxel:quartz",
    "voxel:amethyst", "voxel:packed_snow", "voxel:wet_sand",
    "voxel:amber", "voxel:frost_crystal",
    "voxel:shimmer_stone"
};

static_assert(sizeof(VOXEL_BUILTIN_BLOCK_NAMES)
        / sizeof(VOXEL_BUILTIN_BLOCK_NAMES[0])
        == VOXEL_BUILTIN_BLOCK_COUNT,
    "terrain built-in names must cover every built-in block id");

static ft_bool voxel_block_name_is_valid(const char *name) noexcept
{
    uint32_t index;
    uint32_t separator_index;
    uint32_t separator_count;

    if (name == ft_nullptr || name[0] == '\0')
        return (FT_FALSE);
    index = 0U;
    separator_index = 0U;
    separator_count = 0U;
    while (name[index] != '\0')
    {
        if (name[index] == ':')
        {
            if (separator_count != 0U || index == 0U)
                return (FT_FALSE);
            separator_index = index;
            separator_count += 1U;
        }
        else if (!((name[index] >= 'a' && name[index] <= 'z')
                || (name[index] >= '0' && name[index] <= '9')
                || name[index] == '_'))
            return (FT_FALSE);
        index += 1U;
    }
    if (separator_count != 1U || separator_index + 1U >= index)
        return (FT_FALSE);
    return (FT_TRUE);
}

static pthread_once_t g_voxel_runtime_mutex_once = PTHREAD_ONCE_INIT;
static pt_mutex *g_voxel_runtime_mutex = ft_nullptr;

static void voxel_runtime_initialize_mutex(void) noexcept
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
    g_voxel_runtime_mutex = mutex_pointer;
    return ;
}

static pt_mutex *voxel_runtime_get_mutex(void) noexcept
{
    if (pthread_once(&g_voxel_runtime_mutex_once,
            voxel_runtime_initialize_mutex) != 0)
        return (ft_nullptr);
    return (g_voxel_runtime_mutex);
}

static voxel_runtime_block *voxel_runtime_find_block(
    uint32_t block_id) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;
    voxel_runtime_block *block_pointer;

    if (block_id < VOXEL_RUNTIME_BLOCK_ID_BASE)
        return (ft_nullptr);
    index = block_id - VOXEL_RUNTIME_BLOCK_ID_BASE;
    if (index >= VOXEL_RUNTIME_BLOCK_CAPACITY)
        return (ft_nullptr);
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_nullptr);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    block_pointer = g_voxel_runtime_blocks[index].get();
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (block_pointer);
}

static void voxel_runtime_destroy_block(voxel_runtime_block *block_pointer,
    uint32_t initialized_path_count, uint32_t initialized_data_count) noexcept
{
    (void)initialized_path_count;
    (void)initialized_data_count;
    if (block_pointer == ft_nullptr)
        return ;
    delete block_pointer;
    return ;
}

voxel_runtime_block_handle::voxel_runtime_block_handle() noexcept
    : _block(), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

voxel_runtime_block_handle::~voxel_runtime_block_handle() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t voxel_runtime_block_handle::initialize(
    const voxel_runtime_block_handle &other) noexcept
{
    int32_t error_code;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    error_code = this->_block.initialize(other._block);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_runtime_block_handle::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    error_code = this->_block.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (error_code);
}

int32_t voxel_runtime_block_handle::move(
    voxel_runtime_block_handle &other) noexcept
{
    int32_t error_code;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    error_code = this->_block.move(other._block);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_initialised_state = other._initialised_state;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_runtime_block_handle::is_valid() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_block.get() == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

uint32_t voxel_runtime_block_handle::get_id() const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (0U);
    return (this->_block.get()->block_id);
}

const voxel_block_metadata *voxel_runtime_block_handle::get_metadata() const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (ft_nullptr);
    return (&this->_block.get()->metadata);
}

const char *voxel_runtime_block_handle::get_name() const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (ft_nullptr);
    return (this->_block.get()->name.c_str());
}

const char *voxel_runtime_block_handle::get_asset_path(
    voxel_block_asset_face face) const noexcept
{
    if (this->is_valid() == FT_FALSE
        || face < VOXEL_BLOCK_ASSET_FACE_TOP
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    return (this->_block.get()->asset_paths[face].c_str());
}

const uint8_t *voxel_runtime_block_handle::get_asset_data(
    voxel_block_asset_face face, ft_size_t *size_out) const noexcept
{
    if (size_out == ft_nullptr)
        return (ft_nullptr);
    *size_out = 0U;
    if (this->is_valid() == FT_FALSE
        || face < VOXEL_BLOCK_ASSET_FACE_TOP
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    *size_out = this->_block.get()->asset_data[face].size();
    return (this->_block.get()->asset_data[face].data());
}

static int32_t voxel_runtime_load_asset(const char *path,
    const char *asset_root, ft_byte_buffer &asset_data) noexcept
{
    su_file *file_stream;
    int64_t file_size_long;
    ft_size_t file_size;
    ft_size_t read_count;
    void *temporary_data;
    int32_t error_code;

    if (path == ft_nullptr || path[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_get_type(path) == FILE_TYPE_MISSING)
        return (FT_ERR_FILE_OPEN_FAILED);
    if (file_get_type(path) != FILE_TYPE_REGULAR)
        return (FT_ERR_INVALID_PATH);
    if (asset_root != ft_nullptr)
    {
        error_code = file_validate_regular_file_inside_root(asset_root, path);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = file_get_size(path, &file_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (file_size > VOXEL_RUNTIME_MAX_ASSET_SIZE)
        return (FT_ERR_OUT_OF_RANGE);
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
        if (static_cast<uint64_t>(file_size_long)
            > static_cast<uint64_t>(VOXEL_RUNTIME_MAX_ASSET_SIZE))
            error_code = FT_ERR_OUT_OF_RANGE;
        if (error_code == FT_ERR_SUCCESS)
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
    if (error_code != FT_ERR_SUCCESS)
        (void)asset_data.destroy();
    return (error_code);
}

const voxel_block_metadata *voxel_runtime_find_block_metadata(
    uint32_t block_id) noexcept
{
    voxel_runtime_block *block_pointer;

    block_pointer = voxel_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (&block_pointer->metadata);
}

ft_bool voxel_runtime_block_is_known(uint32_t block_id) noexcept
{
    if (voxel_runtime_find_block(block_id) == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t voxel_register_block_internal(
    const voxel_block_registration &registration, const char *asset_root,
    uint32_t *block_id_out) noexcept
{
    voxel_runtime_block *created_block;
    ft_sharedptr<voxel_runtime_block> created_owner;
    pt_mutex *mutex_pointer;
    uint32_t existing_index;
    uint32_t index;
    uint32_t asset_index;
    uint32_t initialized_path_count;
    uint32_t initialized_data_count;
    int32_t error_code;

    if (block_id_out == ft_nullptr
        || voxel_block_name_is_valid(registration.name) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (registration.metadata.solid > FT_TRUE
        || registration.metadata.transparent > FT_TRUE
        || registration.metadata.liquid > FT_TRUE
        || registration.metadata.replaceable > FT_TRUE
        || registration.metadata.can_host_ore > FT_TRUE
        || registration.metadata.is_ore > FT_TRUE
        || registration.metadata.light_emitting > FT_TRUE
        || registration.metadata.occludes_faces > FT_TRUE
        || registration.metadata.breakable > FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (registration.metadata.is_ore == FT_TRUE
        && registration.metadata.can_host_ore == FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    existing_index = 0U;
    while (existing_index < static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT))
    {
        if (std::strcmp(VOXEL_BUILTIN_BLOCK_NAMES[existing_index],
                registration.name) == 0)
            return (FT_ERR_ALREADY_EXISTS);
        existing_index += 1U;
    }
    asset_index = 0U;
    while (asset_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        if (registration.asset_paths[asset_index] == ft_nullptr
            || registration.asset_paths[asset_index][0] == '\0')
            return (FT_ERR_INVALID_ARGUMENT);
        asset_index += 1U;
    }
    created_block = new (std::nothrow) voxel_runtime_block();
    if (created_block == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    created_block->metadata = registration.metadata;
    error_code = created_block->name.initialize(registration.name);
    asset_index = 0U;
    initialized_path_count = 0U;
    initialized_data_count = 0U;
    while (error_code == FT_ERR_SUCCESS
        && asset_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        error_code = created_block->asset_paths[asset_index].initialize(
            registration.asset_paths[asset_index]);
        if (error_code == FT_ERR_SUCCESS)
            initialized_path_count += 1U;
        if (error_code == FT_ERR_SUCCESS)
            error_code = voxel_runtime_load_asset(
                registration.asset_paths[asset_index], asset_root,
                created_block->asset_data[asset_index]);
        if (error_code == FT_ERR_SUCCESS)
            initialized_data_count += 1U;
        asset_index += 1U;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        voxel_runtime_destroy_block(created_block, initialized_path_count,
            initialized_data_count);
        return (error_code);
    }
    error_code = created_owner.initialize(created_block);
    if (error_code != FT_ERR_SUCCESS)
    {
        delete created_block;
        return (error_code);
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        (void)created_owner.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
    {
        (void)created_owner.destroy();
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    existing_index = 0U;
    while (existing_index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        if (g_voxel_runtime_blocks[existing_index].get() != ft_nullptr
            && std::strcmp(
                g_voxel_runtime_blocks[existing_index].get()->name.c_str(),
                registration.name) == 0)
        {
            (void)pt_mutex_unlock_if_not_null(mutex_pointer);
            (void)created_owner.destroy();
            return (FT_ERR_ALREADY_EXISTS);
        }
        existing_index += 1U;
    }
    index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY
        && g_voxel_runtime_blocks[index].get() != ft_nullptr)
        index += 1U;
    if (index == VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        (void)created_owner.destroy();
        return (FT_ERR_OUT_OF_RANGE);
    }
    created_block->block_id = VOXEL_RUNTIME_BLOCK_ID_BASE + index;
    g_voxel_runtime_blocks[index] = created_owner;
    *block_id_out = created_block->block_id;
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

int32_t voxel_acquire_block(uint32_t block_id,
    voxel_runtime_block_handle &handle) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;
    int32_t error_code;

    if (block_id < VOXEL_RUNTIME_BLOCK_ID_BASE)
        return (FT_ERR_NOT_FOUND);
    index = block_id - VOXEL_RUNTIME_BLOCK_ID_BASE;
    if (index >= VOXEL_RUNTIME_BLOCK_CAPACITY)
        return (FT_ERR_NOT_FOUND);
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (g_voxel_runtime_blocks[index].get() == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NOT_FOUND);
    }
    error_code = handle._block.initialize(g_voxel_runtime_blocks[index]);
    if (error_code == FT_ERR_SUCCESS)
        handle._initialised_state = FT_CLASS_STATE_INITIALISED;
    else
        handle._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (error_code);
}

int32_t voxel_register_block(const voxel_block_registration &registration,
    uint32_t *block_id_out) noexcept
{
    return (voxel_register_block_internal(registration, ft_nullptr,
        block_id_out));
}

int32_t voxel_register_block_from_root(
    const voxel_block_registration &registration, const char *asset_root,
    uint32_t *block_id_out) noexcept
{
    if (asset_root == ft_nullptr || asset_root[0] == '\0')
        return (FT_ERR_INVALID_PATH);
    return (voxel_register_block_internal(registration, asset_root,
        block_id_out));
}

int32_t voxel_unregister_block(uint32_t block_id) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;
    ft_sharedptr<voxel_runtime_block> block_owner;

    if (block_id < VOXEL_RUNTIME_BLOCK_ID_BASE)
        return (FT_ERR_NOT_FOUND);
    index = block_id - VOXEL_RUNTIME_BLOCK_ID_BASE;
    if (index >= VOXEL_RUNTIME_BLOCK_CAPACITY)
        return (FT_ERR_NOT_FOUND);
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (g_voxel_runtime_blocks[index].get() == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NOT_FOUND);
    }
    if (block_owner.initialize(g_voxel_runtime_blocks[index])
        != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NO_MEMORY);
    }
    (void)g_voxel_runtime_blocks[index].destroy();
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

const char *voxel_get_block_name(uint32_t block_id) noexcept
{
    voxel_runtime_block *block_pointer;

    if (block_id < static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT))
        return (VOXEL_BUILTIN_BLOCK_NAMES[block_id]);
    block_pointer = voxel_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (block_pointer->name.c_str());
}

int32_t voxel_find_block_id_by_name(const char *name,
    uint32_t *block_id_out) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;

    if (block_id_out == ft_nullptr
        || voxel_block_name_is_valid(name) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT))
    {
        if (std::strcmp(name, VOXEL_BUILTIN_BLOCK_NAMES[index]) == 0)
        {
            *block_id_out = index;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        if (g_voxel_runtime_blocks[index].get() != ft_nullptr
            && std::strcmp(g_voxel_runtime_blocks[index]->name.c_str(),
                name) == 0)
        {
            *block_id_out = VOXEL_RUNTIME_BLOCK_ID_BASE + index;
            (void)pt_mutex_unlock_if_not_null(mutex_pointer);
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_NOT_FOUND);
}

const char *voxel_get_block_asset_path(uint32_t block_id,
    voxel_block_asset_face face) noexcept
{
    voxel_runtime_block *block_pointer;

    if (face < VOXEL_BLOCK_ASSET_FACE_TOP
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    block_pointer = voxel_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    return (block_pointer->asset_paths[face].c_str());
}

const uint8_t *voxel_get_block_asset_data(uint32_t block_id,
    voxel_block_asset_face face, ft_size_t *size_out) noexcept
{
    voxel_runtime_block *block_pointer;

    if (size_out == ft_nullptr)
        return (ft_nullptr);
    *size_out = 0U;
    if (face < VOXEL_BLOCK_ASSET_FACE_TOP
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    block_pointer = voxel_runtime_find_block(block_id);
    if (block_pointer == ft_nullptr)
        return (ft_nullptr);
    *size_out = block_pointer->asset_data[face].size();
    return (block_pointer->asset_data[face].data());
}

#ifdef LIBFT_TEST_BUILD
void voxel_runtime_reset_for_tests(void) noexcept
{
    uint32_t index;
    pt_mutex *mutex_pointer;
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return ;
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return ;
    index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        (void)g_voxel_runtime_blocks[index].destroy();
        index += 1U;
    }
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return ;
}
#endif

#endif
