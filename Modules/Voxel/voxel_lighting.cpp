#include "voxel_lighting.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "terrain_api.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include <new>
#include <vector>

static const uint16_t VOXEL_LIGHT_SECTION_BLOCKS = 4096U;
static const int32_t VOXEL_LIGHT_HALO = 15;
static const int32_t VOXEL_LIGHT_GRID_EDGE = 16 + VOXEL_LIGHT_HALO * 2;
static const uint8_t VOXEL_LIGHT_UNINITIALISED = 0U;
static const uint8_t VOXEL_LIGHT_INITIALISED = 1U;
static const uint32_t VOXEL_LIGHT_MAX_CONFIG_NODES = 1048576U;
static const uint64_t VOXEL_LIGHT_MAX_CONFIG_TIME_US = 1000000U;

void voxel_light_update_config_defaults(voxel_light_update_config &config) noexcept
{
    config.min_nodes_per_frame = 32U;
    config.target_nodes_per_frame = 128U;
    config.max_nodes_per_frame = 512U;
    config.time_budget_microseconds = 1000U;
    return ;
}

ft_bool voxel_light_update_config_is_valid(
    const voxel_light_update_config &config) noexcept
{
    if (config.min_nodes_per_frame == 0U
        || config.min_nodes_per_frame > config.target_nodes_per_frame
        || config.target_nodes_per_frame > config.max_nodes_per_frame
        || config.max_nodes_per_frame > VOXEL_LIGHT_MAX_CONFIG_NODES
        || config.time_budget_microseconds == 0U
        || config.time_budget_microseconds > VOXEL_LIGHT_MAX_CONFIG_TIME_US)
        return (FT_FALSE);
    return (FT_TRUE);
}

uint8_t voxel_light_pack(uint8_t sky_light, uint8_t block_light) noexcept
{
    if (sky_light > 15U)
        sky_light = 15U;
    if (block_light > 15U)
        block_light = 15U;
    return static_cast<uint8_t>(sky_light | (block_light << 4U));
}

uint8_t voxel_light_sky(uint8_t packed_light) noexcept
{
    return static_cast<uint8_t>(packed_light & 0x0FU);
}

uint8_t voxel_light_block(uint8_t packed_light) noexcept
{
    return static_cast<uint8_t>((packed_light >> 4U) & 0x0FU);
}

uint8_t voxel_light_combined(uint8_t packed_light,
    uint8_t sky_darkening) noexcept
{
    uint8_t sky = voxel_light_sky(packed_light);
    uint8_t block = voxel_light_block(packed_light);
    sky = sky > sky_darkening ? static_cast<uint8_t>(sky - sky_darkening) : 0U;
    return sky > block ? sky : block;
}

voxel_light_section::voxel_light_section() noexcept
    : _uniform_light(0U), _lights(ft_nullptr),
      _initialised_state(VOXEL_LIGHT_UNINITIALISED)
{
}

voxel_light_section::~voxel_light_section() noexcept
{
    (void)destroy();
}

int32_t voxel_light_section::initialize(uint8_t value) noexcept
{
    if (_initialised_state == VOXEL_LIGHT_INITIALISED)
        return FT_ERR_INVALID_OPERATION;
    _uniform_light = value;
    _lights = ft_nullptr;
    _initialised_state = VOXEL_LIGHT_INITIALISED;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_section::destroy() noexcept
{
    delete [] _lights;
    _lights = ft_nullptr;
    _uniform_light = 0U;
    _initialised_state = VOXEL_LIGHT_UNINITIALISED;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_section::move(voxel_light_section &other) noexcept
{
    if (this == &other)
        return FT_ERR_SUCCESS;
    (void)destroy();
    _uniform_light = other._uniform_light;
    _lights = other._lights;
    _initialised_state = other._initialised_state;
    other._uniform_light = 0U;
    other._lights = ft_nullptr;
    other._initialised_state = VOXEL_LIGHT_UNINITIALISED;
    return FT_ERR_SUCCESS;
}

uint8_t voxel_light_section::get(uint16_t index) const noexcept
{
    if (_lights != ft_nullptr)
        return _lights[index];
    return _uniform_light;
}

int32_t voxel_light_section::set(uint16_t index, uint8_t value) noexcept
{
    if (_lights == ft_nullptr && value == _uniform_light)
        return FT_ERR_SUCCESS;
    if (_lights == ft_nullptr)
    {
        _lights = new (std::nothrow) uint8_t[VOXEL_LIGHT_SECTION_BLOCKS];
        if (_lights == ft_nullptr)
            return FT_ERR_NO_MEMORY;
        for (uint16_t i = 0U; i < VOXEL_LIGHT_SECTION_BLOCKS; ++i)
            _lights[i] = _uniform_light;
    }
    _lights[index] = value;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_section::fill(uint8_t value) noexcept
{
    delete [] _lights;
    _lights = ft_nullptr;
    _uniform_light = value;
    return FT_ERR_SUCCESS;
}

ft_bool voxel_light_section::is_uniform() const noexcept
{
    return _lights == ft_nullptr ? FT_TRUE : FT_FALSE;
}

uint8_t voxel_light_section::get_uniform() const noexcept
{
    return _uniform_light;
}

voxel_light_chunk::voxel_light_chunk() noexcept
    : _sections(), _initialised_state(VOXEL_LIGHT_UNINITIALISED)
{
}

voxel_light_chunk::~voxel_light_chunk() noexcept
{
    (void)destroy();
}

int32_t voxel_light_chunk::initialize(uint8_t value) noexcept
{
    uint8_t index;
    if (_initialised_state == VOXEL_LIGHT_INITIALISED)
        return FT_ERR_INVALID_OPERATION;
    index = 0U;
    while (index < 16U)
    {
        int32_t error = _sections[index].initialize(value);
        if (error != FT_ERR_SUCCESS)
        {
            while (index > 0U)
            {
                --index;
                (void)_sections[index].destroy();
            }
            return error;
        }
        ++index;
    }
    _initialised_state = VOXEL_LIGHT_INITIALISED;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_chunk::destroy() noexcept
{
    for (uint8_t i = 0U; i < 16U; ++i)
        (void)_sections[i].destroy();
    _initialised_state = VOXEL_LIGHT_UNINITIALISED;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_chunk::move(voxel_light_chunk &other) noexcept
{
    if (this == &other)
        return FT_ERR_SUCCESS;
    (void)destroy();
    for (uint8_t i = 0U; i < 16U; ++i)
        (void)_sections[i].move(other._sections[i]);
    _initialised_state = other._initialised_state;
    other._initialised_state = VOXEL_LIGHT_UNINITIALISED;
    return FT_ERR_SUCCESS;
}

uint8_t voxel_light_chunk::get(int32_t local_x, int32_t local_y,
    int32_t local_z) const noexcept
{
    uint16_t index = static_cast<uint16_t>((local_y & 15) * 256
        + (local_z & 15) * 16 + (local_x & 15));
    return _sections[static_cast<uint8_t>(local_y >> 4)].get(index);
}

int32_t voxel_light_chunk::set(int32_t local_x, int32_t local_y,
    int32_t local_z, uint8_t value) noexcept
{
    if (local_x < 0 || local_x >= 16 || local_y < 0 || local_y >= 256
        || local_z < 0 || local_z >= 16)
        return FT_ERR_INVALID_ARGUMENT;
    uint16_t index = static_cast<uint16_t>((local_y & 15) * 256
        + local_z * 16 + local_x);
    return _sections[static_cast<uint8_t>(local_y >> 4)].set(index, value);
}

voxel_light_section &voxel_light_chunk::get_section(uint8_t section_index) noexcept
{
    return _sections[section_index & 15U];
}

const voxel_light_section &voxel_light_chunk::get_section(
    uint8_t section_index) const noexcept
{
    return _sections[section_index & 15U];
}

namespace
{
    struct light_node
    {
        int16_t x;
        int16_t y;
        int16_t z;
        uint8_t channel;
        uint8_t level;
    };

    static uint32_t light_index(int32_t x, int32_t y, int32_t z) noexcept
    {
        return static_cast<uint32_t>((y * VOXEL_LIGHT_GRID_EDGE + z)
            * VOXEL_LIGHT_GRID_EDGE + x);
    }

    static int32_t read_block(voxel_light_block_lookup_fn lookup, void *data,
        int32_t x, int32_t y, int32_t z, uint32_t *id) noexcept
    {
        if (y < 0 || y >= 256)
        {
            *id = GAME_VOXEL_AIR_BLOCK;
            return FT_ERR_SUCCESS;
        }
        if (lookup == ft_nullptr)
            return FT_ERR_INVALID_ARGUMENT;
        return lookup(data, x, y, z, id);
    }
}

static int32_t voxel_light_build_chunk_region(voxel_light_chunk &light_chunk,
    int32_t origin_x, int32_t origin_z,
    voxel_light_block_lookup_fn lookup, void *user_data,
    int32_t region_min, int32_t region_max,
    voxel_light_build_stats *stats) noexcept
{
    std::vector<uint8_t> sky(static_cast<size_t>(VOXEL_LIGHT_GRID_EDGE)
        * 256U * VOXEL_LIGHT_GRID_EDGE, 0U);
    std::vector<uint8_t> block(sky.size(), 0U);
    std::vector<light_node> queue;
    uint32_t index;
    int32_t error;

    if (lookup == ft_nullptr)
        return FT_ERR_INVALID_ARGUMENT;
    if (stats != nullptr)
    {
        stats->scanned_cells = 0U;
        stats->propagated_cells = 0U;
        stats->queue_peak = 0U;
    }
    (void)light_chunk.destroy();
    error = light_chunk.initialize(0U);
    if (error != FT_ERR_SUCCESS)
        return error;
    queue.reserve(65536U);
    for (int32_t z = region_min; z < region_max; ++z)
    {
        for (int32_t x = region_min; x < region_max; ++x)
        {
            ft_bool direct = FT_TRUE;
            for (int32_t y = 255; y >= 0; --y)
            {
                uint32_t id = GAME_VOXEL_AIR_BLOCK;
                error = read_block(lookup, user_data, origin_x + x, y,
                    origin_z + z, &id);
                if (error != FT_ERR_SUCCESS)
                    return error;
                if (stats != nullptr)
                    stats->scanned_cells += 1U;
                const terrain_block_metadata &metadata =
                    terrain_get_block_metadata(id);
                index = light_index(x + VOXEL_LIGHT_HALO, y,
                    z + VOXEL_LIGHT_HALO);
                if (direct == FT_TRUE && metadata.light_attenuation < 15U
                    && metadata.transparent == FT_TRUE)
                {
                    sky[index] = 15U;
                        queue.push_back({static_cast<int16_t>(x),
                            static_cast<int16_t>(y), static_cast<int16_t>(z), 0U, 15U});
                }
                else
                    direct = FT_FALSE;
                uint8_t emission = terrain_block_emitted_light_level(id);
                if (emission != 0U)
                {
                    if (emission > block[index])
                    {
                        block[index] = emission;
                        queue.push_back({static_cast<int16_t>(x),
                            static_cast<int16_t>(y), static_cast<int16_t>(z), 1U, emission});
                    }
                }
            }
        }
    }
    if (stats != nullptr)
        stats->queue_peak = queue.size();
    while (!queue.empty())
    {
        light_node node = queue.back();
        queue.pop_back();
        if (stats != nullptr)
            stats->propagated_cells += 1U;
        static const int8_t dx[6] = {1, -1, 0, 0, 0, 0};
        static const int8_t dy[6] = {0, 0, 1, -1, 0, 0};
        static const int8_t dz[6] = {0, 0, 0, 0, 1, -1};
        for (uint8_t direction = 0U; direction < 6U; ++direction)
        {
            int32_t x = node.x + dx[direction];
            int32_t y = node.y + dy[direction];
            int32_t z = node.z + dz[direction];
            if (x < region_min || x >= region_max || y < 0 || y >= 256
                || z < region_min || z >= region_max)
                continue;
            uint32_t id = GAME_VOXEL_AIR_BLOCK;
            error = read_block(lookup, user_data, origin_x + x, y,
                origin_z + z, &id);
            if (error != FT_ERR_SUCCESS)
                return error;
            const terrain_block_metadata &metadata = terrain_get_block_metadata(id);
            if (metadata.light_attenuation >= 15U || metadata.occludes_faces == FT_TRUE)
                continue;
            uint8_t cost = metadata.light_attenuation > 1U
                ? metadata.light_attenuation : 1U;
            if (node.level <= cost)
                continue;
            uint8_t candidate = static_cast<uint8_t>(node.level - cost);
            index = light_index(x + VOXEL_LIGHT_HALO, y,
                z + VOXEL_LIGHT_HALO);
            uint8_t *destination = node.channel == 0U ? &sky[index] : &block[index];
            if (candidate <= *destination)
                continue;
            *destination = candidate;
            queue.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y),
                static_cast<int16_t>(z), node.channel, candidate});
            if (stats != nullptr && queue.size() > stats->queue_peak)
                stats->queue_peak = queue.size();
        }
    }
    for (int32_t y = 0; y < 256; ++y)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t x = 0; x < 16; ++x)
                if (light_chunk.set(x, y, z,
                        voxel_light_pack(sky[light_index(x + VOXEL_LIGHT_HALO,
                            y, z + VOXEL_LIGHT_HALO)],
                            block[light_index(x + VOXEL_LIGHT_HALO, y,
                                z + VOXEL_LIGHT_HALO)])) != FT_ERR_SUCCESS)
                    return FT_ERR_NO_MEMORY;
    return FT_ERR_SUCCESS;
}

int32_t voxel_light_build_chunk(voxel_light_chunk &light_chunk,
    int32_t origin_x, int32_t origin_z,
    voxel_light_block_lookup_fn lookup, void *user_data,
    voxel_light_build_stats *stats) noexcept
{
    return (voxel_light_build_chunk_region(light_chunk, origin_x, origin_z,
        lookup, user_data, -VOXEL_LIGHT_HALO, 16 + VOXEL_LIGHT_HALO, stats));
}

int32_t voxel_light_build_chunk_local(voxel_light_chunk &light_chunk,
    int32_t origin_x, int32_t origin_z,
    voxel_light_block_lookup_fn lookup, void *user_data,
    voxel_light_build_stats *stats) noexcept
{
    return (voxel_light_build_chunk_region(light_chunk, origin_x, origin_z,
        lookup, user_data, 0, 16, stats));
}

#endif
