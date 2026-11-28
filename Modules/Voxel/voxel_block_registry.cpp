#include "voxel_block_registry.hpp"
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Buffer/byte_buffer.hpp"
#include "../File/file_utils.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdio>
#include <array>
#include <cstdlib>
#include <cstring>
#include <new>
#include <pthread.h>

static constexpr uint32_t VOXEL_RUNTIME_BLOCK_ID_BASE =
    static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT);

voxel_runtime_block::voxel_runtime_block() noexcept
    : block_id(0U), handle_references(0U), registry_owned(FT_FALSE),
      metadata(), name(), asset_paths(), asset_data()
{
    return ;
}

voxel_runtime_block::~voxel_runtime_block() noexcept
{
    return ;
}

static voxel_runtime_block *g_voxel_runtime_blocks[
    VOXEL_RUNTIME_BLOCK_CAPACITY] = {};
// The registry mutex protects both the ID slots and the sorted live names.
static voxel_runtime_block *g_voxel_runtime_name_order[VOXEL_RUNTIME_BLOCK_CAPACITY] = {};
static uint32_t g_voxel_runtime_name_count = 0U;
static uint32_t g_voxel_runtime_first_free = 0U;

static constexpr const char *VOXEL_BUILTIN_BLOCK_NAMES[] =
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
    "voxel:shimmer_stone", "voxel:lava", "voxel:flowing_water",
    "voxel:flowing_lava", "voxel:furnace", "voxel:workbench",
    "voxel:paper_mill", "voxel:card_press",
    "voxel:wheat_crop_immature", "voxel:wheat_crop_mature",
    "voxel:farmland", "voxel:paper_reed",
	"voxel:paper_reed_immature", "voxel:sandstone", "voxel:ladder",
	"voxel:trapdoor_closed", "voxel:trapdoor_open",
	"voxel:spider_corpse"
};

static_assert(sizeof(VOXEL_BUILTIN_BLOCK_NAMES)
        / sizeof(VOXEL_BUILTIN_BLOCK_NAMES[0])
        == VOXEL_BUILTIN_BLOCK_COUNT,
    "voxel built-in names must cover every built-in block id");

static constexpr ft_bool voxel_builtin_name_less(const char *left, const char *right) noexcept
{
    while (*left == *right && *left != '\0')
    {
        ++left;
        ++right;
    }
    return (static_cast<uint8_t>(*left) < static_cast<uint8_t>(*right));
}

static constexpr std::array<uint32_t, VOXEL_BUILTIN_BLOCK_COUNT>
voxel_make_builtin_name_order() noexcept
{
    std::array<uint32_t, VOXEL_BUILTIN_BLOCK_COUNT> order = {};
    uint32_t block_id = 0U;
    while (block_id < VOXEL_BUILTIN_BLOCK_COUNT)
    {
        uint32_t position = block_id;
        while (position != 0U && voxel_builtin_name_less(VOXEL_BUILTIN_BLOCK_NAMES[block_id],
                VOXEL_BUILTIN_BLOCK_NAMES[order[position - 1U]]))
        {
            order[position] = order[position - 1U];
            --position;
        }
        order[position] = block_id;
        ++block_id;
    }
    return (order);
}

static constexpr auto VOXEL_BUILTIN_NAME_ORDER = voxel_make_builtin_name_order();

static constexpr ft_bool voxel_builtin_names_are_unique() noexcept
{
    uint32_t index = 1U;
    while (index < VOXEL_BUILTIN_BLOCK_COUNT)
    {
        if (voxel_builtin_name_less(VOXEL_BUILTIN_BLOCK_NAMES[VOXEL_BUILTIN_NAME_ORDER[index - 1U]],
                VOXEL_BUILTIN_BLOCK_NAMES[VOXEL_BUILTIN_NAME_ORDER[index]]) == FT_FALSE)
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

static_assert(voxel_builtin_names_are_unique(), "voxel built-in names must be unique");

static uint32_t voxel_find_builtin_block_name(const char *name) noexcept
{
    uint32_t first = 0U;
    uint32_t last = VOXEL_BUILTIN_BLOCK_COUNT;
    while (first < last)
    {
        const uint32_t middle = first + (last - first) / 2U;
        const uint32_t block_id = VOXEL_BUILTIN_NAME_ORDER[middle];
        const int32_t comparison = std::strcmp(name, VOXEL_BUILTIN_BLOCK_NAMES[block_id]);
        if (comparison == 0)
            return (block_id);
        if (comparison < 0)
            last = middle;
        else
            first = middle + 1U;
    }
    return (VOXEL_BUILTIN_BLOCK_COUNT);
}

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
    block_pointer = g_voxel_runtime_blocks[index];
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (block_pointer);
}

static void voxel_runtime_destroy_block(voxel_runtime_block *block_pointer,
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
    block_pointer->~voxel_runtime_block();
    std::free(block_pointer);
    return ;
}

voxel_runtime_block_handle::voxel_runtime_block_handle() noexcept
    : _block(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
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
    pt_mutex *mutex_pointer;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "voxel_runtime_block_handle::initialize",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (this->destroy() != FT_ERR_SUCCESS)
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_block = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    this->_block = other._block;
    if (this->_block == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_INVALID_STATE);
    }
    this->_block->handle_references.fetch_add(1U,
        std::memory_order_relaxed);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

int32_t voxel_runtime_block_handle::destroy() noexcept
{
    pt_mutex *mutex_pointer;
    voxel_runtime_block *block_pointer;
    uint32_t references;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    block_pointer = this->_block;
    this->_block = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    references = block_pointer->handle_references.fetch_sub(1U,
        std::memory_order_relaxed) - 1U;
    if (references == 0U && block_pointer->registry_owned == FT_FALSE)
        voxel_runtime_destroy_block(block_pointer,
            VOXEL_BLOCK_ASSET_FACE_COUNT);
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

int32_t voxel_runtime_block_handle::move(
    voxel_runtime_block_handle &other) noexcept
{
    int32_t error_code;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "voxel_runtime_block_handle::move",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    error_code = this->destroy();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_block = other._block;
    this->_initialised_state = other._initialised_state;
    other._block = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_runtime_block_handle::is_valid() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED
        && this->_block != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

uint32_t voxel_runtime_block_handle::get_id() const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (0U);
    return (this->_block->block_id);
}

const voxel_block_metadata *voxel_runtime_block_handle::get_metadata()
    const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (ft_nullptr);
    return (&this->_block->metadata);
}

const char *voxel_runtime_block_handle::get_name() const noexcept
{
    if (this->is_valid() == FT_FALSE)
        return (ft_nullptr);
    return (this->_block->name.c_str());
}

const char *voxel_runtime_block_handle::get_asset_path(
    voxel_block_asset_face face) const noexcept
{
    if (this->is_valid() == FT_FALSE
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    return (this->_block->asset_paths[face].c_str());
}

const uint8_t *voxel_runtime_block_handle::get_asset_data(
    voxel_block_asset_face face, ft_size_t *size_out) const noexcept
{
    if (size_out == ft_nullptr)
        return (ft_nullptr);
    *size_out = 0U;
    if (this->is_valid() == FT_FALSE
        || face >= VOXEL_BLOCK_ASSET_FACE_COUNT)
        return (ft_nullptr);
    *size_out = this->_block->asset_data[face].size();
    return (this->_block->asset_data[face].data());
}

int32_t voxel_acquire_block(uint32_t block_id,
    voxel_runtime_block_handle &handle) noexcept
{
    pt_mutex *mutex_pointer;
    voxel_runtime_block *block_pointer;
    uint32_t index;

    if (block_id < VOXEL_RUNTIME_BLOCK_ID_BASE)
        return (FT_ERR_NOT_FOUND);
    index = block_id - VOXEL_RUNTIME_BLOCK_ID_BASE;
    if (index >= VOXEL_RUNTIME_BLOCK_CAPACITY)
        return (FT_ERR_NOT_FOUND);
    if (handle._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (handle.destroy() != FT_ERR_SUCCESS)
            return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    block_pointer = g_voxel_runtime_blocks[index];
    if (block_pointer == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NOT_FOUND);
    }
    block_pointer->handle_references.fetch_add(1U,
        std::memory_order_relaxed);
    handle._block = block_pointer;
    handle._initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_runtime_read_asset(uint8_t *destination,
    ft_size_t length, void *user_data) noexcept
{
    su_file *file_stream = static_cast<su_file *>(user_data);
    if (su_fread(destination, 1U, length, file_stream) != length)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_runtime_load_asset(const char *path,
    ft_byte_buffer &asset_data) noexcept
{
    su_file *file_stream;
    int64_t file_size_long;
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
    if (error_code == FT_ERR_SUCCESS)
        error_code = asset_data.initialize_from_reader(
            static_cast<ft_size_t>(file_size_long), voxel_runtime_read_asset, file_stream);
    if (su_fclose(file_stream) != 0 && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
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

static uint32_t voxel_runtime_name_position_locked(const char *name) noexcept
{
    uint32_t first = 0U;
    uint32_t last = g_voxel_runtime_name_count;
    while (first < last)
    {
        const uint32_t middle = first + (last - first) / 2U;
        if (std::strcmp(g_voxel_runtime_name_order[middle]->name.c_str(), name) < 0)
            first = middle + 1U;
        else
            last = middle;
    }
    return (first);
}

static ft_bool voxel_runtime_name_matches_locked(uint32_t position, const char *name) noexcept
{
    return (position < g_voxel_runtime_name_count
        && std::strcmp(g_voxel_runtime_name_order[position]->name.c_str(), name) == 0);
}

static int32_t voxel_find_registration_slot_locked(const char *name, uint32_t &position) noexcept
{
    position = voxel_runtime_name_position_locked(name);
    if (voxel_runtime_name_matches_locked(position, name) == FT_TRUE)
        return (FT_ERR_ALREADY_EXISTS);
    if (g_voxel_runtime_first_free == VOXEL_RUNTIME_BLOCK_CAPACITY)
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_register_block_internal(
    const voxel_block_registration &registration, const char *asset_root,
    uint32_t *block_id_out) noexcept
{
    voxel_runtime_block *created_block;
    void *memory_pointer;
    pt_mutex *mutex_pointer;
    uint32_t name_position;
    uint32_t asset_index;
    ft_string *joined_asset_path;
    const char *asset_path;
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
        || registration.metadata.breakable > FT_TRUE
        || registration.metadata.emitted_light_level > 15U
        || registration.metadata.light_attenuation > 15U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (registration.metadata.is_ore == FT_TRUE
        && registration.metadata.can_host_ore == FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (voxel_find_builtin_block_name(registration.name) != VOXEL_BUILTIN_BLOCK_COUNT)
        return (FT_ERR_ALREADY_EXISTS);
    asset_index = 0U;
    while (asset_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        if (registration.asset_paths[asset_index] == ft_nullptr
            || registration.asset_paths[asset_index][0] == '\0')
            return (FT_ERR_INVALID_ARGUMENT);
        asset_index += 1U;
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    error_code = voxel_find_registration_slot_locked(registration.name, name_position);
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    // Keep file I/O outside the registry lock and recheck the slot at publication.
    memory_pointer = std::malloc(sizeof(voxel_runtime_block));
    if (memory_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    created_block = new (memory_pointer) voxel_runtime_block();
    created_block->metadata = registration.metadata;
    error_code = created_block->name.initialize(registration.name);
    asset_index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && asset_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        error_code = created_block->asset_paths[asset_index].initialize(
            registration.asset_paths[asset_index]);
        joined_asset_path = ft_nullptr;
        if (error_code == FT_ERR_SUCCESS && asset_root != ft_nullptr)
        {
            joined_asset_path = file_path_join(asset_root,
                registration.asset_paths[asset_index]);
            if (joined_asset_path == ft_nullptr)
                error_code = FT_ERR_NO_MEMORY;
            else if (joined_asset_path->get_error() != FT_ERR_SUCCESS)
                error_code = joined_asset_path->get_error();
        }
        if (error_code == FT_ERR_SUCCESS)
        {
            asset_path = registration.asset_paths[asset_index];
            if (joined_asset_path != ft_nullptr)
                asset_path = joined_asset_path->c_str();
            error_code = voxel_runtime_load_asset(
                asset_path,
                created_block->asset_data[asset_index]);
        }
        if (joined_asset_path != ft_nullptr)
        {
            (void)joined_asset_path->destroy();
            delete joined_asset_path;
        }
        asset_index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        error_code = pt_mutex_lock_if_not_null(mutex_pointer);
        if (error_code == FT_ERR_SUCCESS)
        {
            error_code = voxel_find_registration_slot_locked(
                created_block->name.c_str(), name_position);
            if (error_code == FT_ERR_SUCCESS)
            {
                const uint32_t index = g_voxel_runtime_first_free;
                created_block->block_id = VOXEL_RUNTIME_BLOCK_ID_BASE + index;
                created_block->registry_owned = FT_TRUE;
                g_voxel_runtime_blocks[index] = created_block;
                const uint32_t tail_count = g_voxel_runtime_name_count - name_position;
                if (tail_count != 0U)
                    (void)ft_memmove(g_voxel_runtime_name_order + name_position + 1U,
                        g_voxel_runtime_name_order + name_position,
                        tail_count * sizeof(g_voxel_runtime_name_order[0]));
                g_voxel_runtime_name_order[name_position] = created_block;
                ++g_voxel_runtime_name_count;
                do
                {
                    ++g_voxel_runtime_first_free;
                }
                while (g_voxel_runtime_first_free < VOXEL_RUNTIME_BLOCK_CAPACITY
                    && g_voxel_runtime_blocks[g_voxel_runtime_first_free] != ft_nullptr);
                *block_id_out = created_block->block_id;
            }
            (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        }
        else
            error_code = FT_ERR_SYS_MUTEX_LOCK_FAILED;
    }
    if (error_code != FT_ERR_SUCCESS)
        voxel_runtime_destroy_block(created_block, asset_index);
    return (error_code);
}

int32_t voxel_register_block(const voxel_block_registration &registration,
    uint32_t *block_id_out) noexcept
{
    return (voxel_register_block_internal(registration, ft_nullptr,
        block_id_out));
}

int32_t voxel_unregister_block(uint32_t block_id) noexcept
{
    pt_mutex *mutex_pointer;
    voxel_runtime_block *block_pointer;
    uint32_t index;
    uint32_t references;

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
    block_pointer = g_voxel_runtime_blocks[index];
    if (block_pointer == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_NOT_FOUND);
    }
    const uint32_t position = voxel_runtime_name_position_locked(block_pointer->name.c_str());
    --g_voxel_runtime_name_count;
    const uint32_t tail_count = g_voxel_runtime_name_count - position;
    if (tail_count != 0U)
        (void)ft_memmove(g_voxel_runtime_name_order + position,
            g_voxel_runtime_name_order + position + 1U,
            tail_count * sizeof(g_voxel_runtime_name_order[0]));
    g_voxel_runtime_name_order[g_voxel_runtime_name_count] = ft_nullptr;
    g_voxel_runtime_blocks[index] = ft_nullptr;
    if (index < g_voxel_runtime_first_free)
        g_voxel_runtime_first_free = index;
    block_pointer->registry_owned = FT_FALSE;
    references = block_pointer->handle_references.load(
        std::memory_order_relaxed);
    if (references == 0U)
        voxel_runtime_destroy_block(block_pointer,
            VOXEL_BLOCK_ASSET_FACE_COUNT);
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return (FT_ERR_SUCCESS);
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
    pt_mutex *mutex_pointer;

    if (block_id_out == ft_nullptr
        || voxel_block_name_is_valid(name) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t builtin_id = voxel_find_builtin_block_name(name);
    if (builtin_id != VOXEL_BUILTIN_BLOCK_COUNT)
    {
        *block_id_out = builtin_id;
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    const uint32_t position = voxel_runtime_name_position_locked(name);
    if (voxel_runtime_name_matches_locked(position, name) == FT_TRUE)
    {
        *block_id_out = g_voxel_runtime_name_order[position]->block_id;
        (void)pt_mutex_unlock_if_not_null(mutex_pointer);
        return (FT_ERR_SUCCESS);
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
    voxel_runtime_block *block_pointer;

    mutex_pointer = voxel_runtime_get_mutex();
    if (mutex_pointer == ft_nullptr)
        return ;
    if (pt_mutex_lock_if_not_null(mutex_pointer) != FT_ERR_SUCCESS)
        return ;
    g_voxel_runtime_first_free = 0U;
    g_voxel_runtime_name_count = 0U;
    index = 0U;
    while (index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        block_pointer = g_voxel_runtime_blocks[index];
        g_voxel_runtime_blocks[index] = ft_nullptr;
        g_voxel_runtime_name_order[index] = ft_nullptr;
        if (block_pointer != ft_nullptr)
        {
            block_pointer->registry_owned = FT_FALSE;
            if (block_pointer->handle_references.load(
                    std::memory_order_relaxed) == 0U)
                voxel_runtime_destroy_block(block_pointer,
                    VOXEL_BLOCK_ASSET_FACE_COUNT);
        }
        index += 1U;
    }
    (void)pt_mutex_unlock_if_not_null(mutex_pointer);
    return ;
}
#endif

#endif
