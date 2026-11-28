#include "voxel_lighting.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_api.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../PThread/pt_thread_workspace.hpp"
#include <chrono>
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

int32_t voxel_light_section::assign_dense(
    const uint8_t (&values)[VOXEL_LIGHT_SECTION_BLOCKS]) noexcept
{
    if (this->_lights == ft_nullptr)
    {
        this->_lights = new (std::nothrow) uint8_t[VOXEL_LIGHT_SECTION_BLOCKS];
        if (this->_lights == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    ft_memcpy(this->_lights, values, sizeof(values));
    return (FT_ERR_SUCCESS);
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

    /* World-generation workers are long-lived. Reuse their lighting scratch
     * storage instead of allocating two half-million-cell grids and a large
     * propagation queue for every remesh. These buffers are thread-local, so
     * concurrent workers never share mutable solver state. */
    struct light_workspace
    {
        std::vector<uint8_t> sky;
        std::vector<uint8_t> block;
        std::vector<uint8_t> sky_queued;
        std::vector<uint8_t> block_queued;
        std::vector<uint8_t> step_costs;
        std::vector<light_node> queue;
    };

    static uint32_t light_grid_index(int32_t x, int32_t y, int32_t z,
        int32_t edge) noexcept
    {
        return (static_cast<uint32_t>((y * edge + z) * edge + x));
    }

    static uint8_t light_step_cost(const voxel_block_metadata &metadata) noexcept
    {
        if (metadata.light_attenuation >= 15U || metadata.occludes_faces == FT_TRUE)
            return (15U);
        if (metadata.light_attenuation == 0U)
            return (1U);
        return (metadata.light_attenuation);
    }

    static int32_t light_store_section(voxel_light_section &section,
        const uint8_t (&packed)[VOXEL_LIGHT_SECTION_BLOCKS], uint8_t differences) noexcept
    {
        if (differences == 0U)
            return (section.fill(packed[0]));
        return (section.assign_dense(packed));
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

    static int32_t light_queue_push(std::vector<light_node> &queue,
        const light_node &node) noexcept
    {
        try
        {
            queue.push_back(node);
        }
        catch (const std::bad_alloc &)
        {
            return (FT_ERR_NO_MEMORY);
        }
        return (FT_ERR_SUCCESS);
    }

    static int32_t light_seed_sky_column(light_workspace &workspace,
        int32_t x, int32_t z, int32_t first_y, int32_t end_y,
        int32_t region_min, int32_t edge) noexcept
    {
        int32_t y = first_y;
        while (y < end_y)
        {
            const uint32_t index = light_grid_index(x - region_min, y, z - region_min, edge);
            const uint8_t cost = workspace.step_costs[index];
            if (cost < 15U)
            {
                const uint8_t candidate = static_cast<uint8_t>(15U - cost);
                if (candidate > workspace.sky[index])
                {
                    if (candidate > 1U)
                    {
                        const int32_t error_code = light_queue_push(workspace.queue, {
                            static_cast<int16_t>(x), static_cast<int16_t>(y),
                            static_cast<int16_t>(z), 0U, candidate});
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                        workspace.sky_queued[index] = 1U;
                    }
                    workspace.sky[index] = candidate;
                }
            }
            y += 1;
        }
        return (FT_ERR_SUCCESS);
    }

    static int32_t light_seed_sky_boundaries(light_workspace &workspace,
        const uint16_t *sky_floors, int32_t region_min, int32_t region_max) noexcept
    {
        const int32_t edge = region_max - region_min;
        int32_t z = region_min;
        uint32_t column = 0U;
        while (z < region_max)
        {
            int32_t x = region_min;
            while (x < region_max)
            {
                const int32_t floor = sky_floors[column];
                int32_t error_code = FT_ERR_SUCCESS;
                if (floor > 0 && floor < 256)
                    error_code = light_seed_sky_column(workspace, x, z,
                        floor - 1, floor, region_min, edge);
                if (error_code == FT_ERR_SUCCESS && x > region_min)
                {
                    const int32_t west_floor = sky_floors[column - 1U];
                    if (west_floor < floor)
                        error_code = light_seed_sky_column(workspace, x, z,
                            west_floor, floor, region_min, edge);
                    else if (floor < west_floor)
                        error_code = light_seed_sky_column(workspace, x - 1, z,
                            floor, west_floor, region_min, edge);
                }
                if (error_code == FT_ERR_SUCCESS && z > region_min)
                {
                    const int32_t north_floor = sky_floors[column - edge];
                    if (north_floor < floor)
                        error_code = light_seed_sky_column(workspace, x, z,
                            north_floor, floor, region_min, edge);
                    else if (floor < north_floor)
                        error_code = light_seed_sky_column(workspace, x, z - 1,
                            floor, north_floor, region_min, edge);
                }
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                column += 1U;
                x += 1;
            }
            z += 1;
        }
        return (FT_ERR_SUCCESS);
    }
}

struct voxel_light_build_operation::implementation
{
    voxel_light_chunk *light_chunk;
    int32_t origin_x;
    int32_t origin_z;
    int32_t region_min;
    int32_t region_max;
    int32_t grid_edge;
    int32_t scan_x;
    int32_t scan_y;
    int32_t scan_z;
    ft_bool direct_sky;
    uint16_t final_cell;
    uint8_t final_section;
    uint8_t final_differences;
    uint8_t final_lights[VOXEL_LIGHT_SECTION_BLOCKS];
    uint8_t phase;
    uint32_t last_block_id;
    uint8_t last_block_cost;
    uint8_t last_block_emission;
    ft_bool last_block_passes_sky;
    ft_bool has_last_block;
    voxel_light_block_lookup_fn lookup;
    void *user_data;
    voxel_light_build_stats stats;
    std::vector<uint8_t> sky;
    std::vector<uint8_t> block;
    std::vector<uint8_t> step_costs;
    std::vector<light_node> queues[16];
    uint32_t highest_queued_level;
    size_t queued_count;

    int32_t enqueue(const light_node &node) noexcept;
    uint32_t index(int32_t x, int32_t y, int32_t z) const noexcept;
    int32_t seed_sky_neighbor(uint32_t destination_index, int32_t destination_x,
        int32_t destination_y, int32_t destination_z) noexcept;
    int32_t propagate_next() noexcept;
    int32_t finalize_next() noexcept;
};

static const uint8_t VOXEL_LIGHT_OPERATION_SCAN = 0U;
static const uint8_t VOXEL_LIGHT_OPERATION_PROPAGATE = 1U;
static const uint8_t VOXEL_LIGHT_OPERATION_FINALIZE = 2U;
static const uint8_t VOXEL_LIGHT_OPERATION_COMPLETE = 3U;

int32_t voxel_light_build_operation::implementation::enqueue(
    const light_node &node) noexcept
{
    int32_t error_code;

    if (node.level <= 1U)
        return (FT_ERR_SUCCESS);
    error_code = light_queue_push(this->queues[node.level], node);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (node.level > this->highest_queued_level)
        this->highest_queued_level = node.level;
    this->queued_count += 1U;
    if (this->queued_count > this->stats.queue_peak)
        this->stats.queue_peak = this->queued_count;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_light_build_operation::implementation::index(
    int32_t x, int32_t y, int32_t z) const noexcept
{
    return (light_grid_index(x - this->region_min, y, z - this->region_min,
        this->grid_edge));
}

int32_t voxel_light_build_operation::implementation::seed_sky_neighbor(
    uint32_t destination_index, int32_t destination_x, int32_t destination_y,
    int32_t destination_z) noexcept
{
    uint8_t cost;
    uint8_t candidate;

    cost = this->step_costs[destination_index];
    if (cost >= 15U)
        return (FT_ERR_SUCCESS);
    candidate = static_cast<uint8_t>(15U - cost);
    if (candidate <= this->sky[destination_index])
        return (FT_ERR_SUCCESS);
    this->sky[destination_index] = candidate;
    return (this->enqueue({
        static_cast<int16_t>(destination_x),
        static_cast<int16_t>(destination_y),
        static_cast<int16_t>(destination_z), 0U, candidate}));
}

int32_t voxel_light_build_operation::implementation::propagate_next() noexcept
{
    static const int8_t DELTA_X[6] = {1, -1, 0, 0, 0, 0};
    static const int8_t DELTA_Y[6] = {0, 0, 1, -1, 0, 0};
    static const int8_t DELTA_Z[6] = {0, 0, 0, 0, 1, -1};
    uint8_t direction;

    /* Every step loses light. Process stronger sources first so a settled
     * cell never needs to spread again. Earlier, weaker entries can be stale. */
    while (this->queues[this->highest_queued_level].empty())
        this->highest_queued_level -= 1U;
    std::vector<light_node> &queue = this->queues[this->highest_queued_level];
    const light_node node = queue.back();
    queue.pop_back();
    this->queued_count -= 1U;
    std::vector<uint8_t> *channel = &this->sky;
    if (node.channel != 0U)
        channel = &this->block;
    if ((*channel)[this->index(node.x, node.y, node.z)] != node.level)
        return (FT_ERR_SUCCESS);
    this->stats.propagated_cells += 1U;
    direction = 0U;
    while (direction < 6U)
    {
        const int32_t coordinate_x = node.x + DELTA_X[direction];
        const int32_t coordinate_y = node.y + DELTA_Y[direction];
        const int32_t coordinate_z = node.z + DELTA_Z[direction];
        direction += 1U;
        if (coordinate_x < this->region_min || coordinate_x >= this->region_max
            || coordinate_y < 0 || coordinate_y >= 256
            || coordinate_z < this->region_min || coordinate_z >= this->region_max)
            continue ;
        const uint32_t index = this->index(coordinate_x, coordinate_y, coordinate_z);
        const uint8_t cost = this->step_costs[index];
        if (node.level <= cost)
            continue ;
        const uint8_t candidate = static_cast<uint8_t>(node.level - cost);
        if (candidate <= (*channel)[index])
            continue ;
        (*channel)[index] = candidate;
        const int32_t error_code = this->enqueue({
            static_cast<int16_t>(coordinate_x),
            static_cast<int16_t>(coordinate_y),
            static_cast<int16_t>(coordinate_z), node.channel, candidate});
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_light_build_operation::implementation::finalize_next() noexcept
{
    const int32_t x = this->final_cell & 15U;
    const int32_t z = (this->final_cell >> 4U) & 15U;
    const int32_t y = (this->final_section << 4U) + (this->final_cell >> 8U);
    const uint32_t index = this->index(x, y, z);
    const uint8_t packed = static_cast<uint8_t>(this->sky[index] | (this->block[index] << 4U));
    this->final_lights[this->final_cell] = packed;
    this->final_differences |= packed ^ this->final_lights[0];
    if (this->final_cell + 1U < VOXEL_LIGHT_SECTION_BLOCKS)
    {
        this->final_cell += 1U;
        return (FT_ERR_SUCCESS);
    }
    const int32_t error = light_store_section(this->light_chunk->get_section(this->final_section),
        this->final_lights, this->final_differences);
    if (error != FT_ERR_SUCCESS)
        return (error);
    this->final_cell = 0U;
    this->final_differences = 0U;
    this->final_section += 1U;
    if (this->final_section == GAME_VOXEL_CHUNK_SECTION_COUNT)
        this->phase = VOXEL_LIGHT_OPERATION_COMPLETE;
    return (FT_ERR_SUCCESS);
}

voxel_light_build_operation::voxel_light_build_operation() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _implementation(ft_nullptr)
{
    return ;
}

voxel_light_build_operation::~voxel_light_build_operation() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t voxel_light_build_operation::initialize(
    voxel_light_chunk &light_chunk, int32_t world_origin_x,
    int32_t world_origin_z, voxel_light_block_lookup_fn lookup_block,
    void *user_data, ft_bool include_halo) noexcept
{
    implementation *operation;
    size_t grid_size;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    if (lookup_block == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation = new (std::nothrow) implementation();
    if (operation == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    operation->light_chunk = &light_chunk;
    operation->origin_x = world_origin_x;
    operation->origin_z = world_origin_z;
    operation->region_min = include_halo != FT_FALSE ? -VOXEL_LIGHT_HALO : 0;
    operation->region_max = include_halo != FT_FALSE
        ? 16 + VOXEL_LIGHT_HALO : 16;
    operation->grid_edge = operation->region_max - operation->region_min;
    operation->scan_x = operation->region_min;
    operation->scan_y = 255;
    operation->scan_z = operation->region_min;
    operation->direct_sky = FT_TRUE;
    operation->final_cell = 0U;
    operation->final_section = 0U;
    operation->final_differences = 0U;
    operation->phase = VOXEL_LIGHT_OPERATION_SCAN;
    operation->lookup = lookup_block;
    operation->user_data = user_data;
    operation->stats = {};
    grid_size = static_cast<size_t>(operation->grid_edge) * 256U
        * operation->grid_edge;
    try
    {
        operation->sky.assign(grid_size, 0U);
        operation->block.assign(grid_size, 0U);
        operation->step_costs.assign(grid_size, 15U);
    }
    catch (const std::bad_alloc &)
    {
        delete operation;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    error_code = light_chunk.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = light_chunk.initialize(0U);
    if (error_code != FT_ERR_SUCCESS)
    {
        delete operation;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_implementation = operation;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_light_build_operation::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    delete this->_implementation;
    this->_implementation = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_light_build_operation::move(
    voxel_light_build_operation &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_implementation = other._implementation;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._implementation = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_light_build_operation::step(
    const voxel_light_update_config &config, voxel_light_build_stats *stats,
    ft_bool *complete) noexcept
{
    implementation *operation;
    uint32_t processed;
    uint32_t minimum_nodes;
    uint32_t target_nodes;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_implementation == ft_nullptr || complete == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (voxel_light_update_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    operation = this->_implementation;
    *complete = operation->phase == VOXEL_LIGHT_OPERATION_COMPLETE
        ? FT_TRUE : FT_FALSE;
    if (*complete != FT_FALSE)
    {
        if (stats != nullptr)
            *stats = operation->stats;
        return (FT_ERR_SUCCESS);
    }
    minimum_nodes = config.min_nodes_per_frame;
    target_nodes = config.target_nodes_per_frame;
    processed = 0U;
    const ft_bool use_time_budget = minimum_nodes < target_nodes;
    std::chrono::steady_clock::time_point start;
    if (use_time_budget != FT_FALSE)
        start = std::chrono::steady_clock::now();
    while (processed < target_nodes)
    {
        if (operation->phase == VOXEL_LIGHT_OPERATION_SCAN)
        {
            uint32_t block_id;
            uint32_t index;

            block_id = GAME_VOXEL_AIR_BLOCK;
            if (operation->lookup(operation->user_data,
                    operation->origin_x + operation->scan_x,
                    operation->scan_y,
                    operation->origin_z + operation->scan_z,
                    &block_id) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            index = operation->index(operation->scan_x, operation->scan_y, operation->scan_z);
            operation->stats.scanned_cells += 1U;
            if (operation->has_last_block == FT_FALSE
                || operation->last_block_id != block_id)
            {
                const voxel_block_metadata &metadata =
                    voxel_get_block_metadata(block_id);
                operation->has_last_block = FT_TRUE;
                operation->last_block_id = block_id;
                operation->last_block_cost = light_step_cost(metadata);
                operation->last_block_passes_sky = FT_FALSE;
                if (metadata.light_attenuation < 15U
                    && metadata.transparent != FT_FALSE)
                    operation->last_block_passes_sky = FT_TRUE;
                operation->last_block_emission =
                    voxel_block_emitted_light_level(block_id);
            }
            operation->step_costs[index] = operation->last_block_cost;
            if (operation->direct_sky != FT_FALSE
                && operation->last_block_passes_sky != FT_FALSE)
            {
                operation->sky[index] = 15U;
                if (operation->scan_x > operation->region_min)
                {
                    const uint32_t west_index = index - 1U;
                    if (operation->sky[west_index] == 0U)
                    {
                        error_code = operation->seed_sky_neighbor(west_index,
                            operation->scan_x - 1, operation->scan_y,
                            operation->scan_z);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
                if (operation->scan_z > operation->region_min)
                {
                    const uint32_t north_index = index - operation->grid_edge;
                    if (operation->sky[north_index] == 0U)
                    {
                        error_code = operation->seed_sky_neighbor(north_index,
                            operation->scan_x, operation->scan_y,
                            operation->scan_z - 1);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            else
            {
                if (operation->last_block_cost < 15U)
                {
                    if (operation->direct_sky != FT_FALSE && operation->scan_y < 255)
                    {
                        error_code = operation->seed_sky_neighbor(index,
                            operation->scan_x, operation->scan_y, operation->scan_z);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                    if (operation->scan_x > operation->region_min)
                    {
                        const uint32_t west_index = index - 1U;
                        if (operation->sky[west_index] == 15U)
                        {
                            error_code = operation->seed_sky_neighbor(index,
                                operation->scan_x, operation->scan_y,
                                operation->scan_z);
                            if (error_code != FT_ERR_SUCCESS)
                                return (error_code);
                        }
                    }
                    if (operation->scan_z > operation->region_min)
                    {
                        const uint32_t north_index = index - operation->grid_edge;
                        if (operation->sky[north_index] == 15U)
                        {
                            error_code = operation->seed_sky_neighbor(index,
                                operation->scan_x, operation->scan_y,
                                operation->scan_z);
                            if (error_code != FT_ERR_SUCCESS)
                                return (error_code);
                        }
                    }
                }
                operation->direct_sky = FT_FALSE;
            }
            if (operation->last_block_emission > 0U)
            {
                operation->block[index] = operation->last_block_emission;
                error_code = operation->enqueue({
                    static_cast<int16_t>(operation->scan_x),
                    static_cast<int16_t>(operation->scan_y),
                    static_cast<int16_t>(operation->scan_z), 1U,
                    operation->last_block_emission});
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
            operation->scan_y -= 1;
            if (operation->scan_y < 0)
            {
                operation->scan_y = 255;
                operation->scan_x += 1;
                operation->direct_sky = FT_TRUE;
                if (operation->scan_x >= operation->region_max)
                {
                    operation->scan_x = operation->region_min;
                    operation->scan_z += 1;
                    if (operation->scan_z >= operation->region_max)
                        operation->phase = VOXEL_LIGHT_OPERATION_PROPAGATE;
                }
            }
        }
        else if (operation->phase == VOXEL_LIGHT_OPERATION_PROPAGATE)
        {
            if (operation->queued_count == 0U)
            {
                operation->phase = VOXEL_LIGHT_OPERATION_FINALIZE;
                continue ;
            }
            error_code = operation->propagate_next();
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        else if (operation->phase == VOXEL_LIGHT_OPERATION_FINALIZE)
        {
            error_code = operation->finalize_next();
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            if (operation->phase == VOXEL_LIGHT_OPERATION_COMPLETE)
                break ;
        }
        processed += 1U;
        if (use_time_budget != FT_FALSE && processed < target_nodes
            && processed >= minimum_nodes
            && (processed == minimum_nodes || (processed & 31U) == 0U))
        {
            if (std::chrono::steady_clock::now() - start
                >= std::chrono::microseconds(config.time_budget_microseconds))
                break ;
        }
    }
    *complete = operation->phase == VOXEL_LIGHT_OPERATION_COMPLETE
        ? FT_TRUE : FT_FALSE;
    if (stats != nullptr)
        *stats = operation->stats;
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_light_build_operation::is_complete() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_implementation == ft_nullptr)
        return (FT_FALSE);
    return (this->_implementation->phase == VOXEL_LIGHT_OPERATION_COMPLETE
        ? FT_TRUE : FT_FALSE);
}

int32_t voxel_light_build_operation::get_packed_light(int32_t world_x,
    int32_t world_y, int32_t world_z, uint8_t *packed_light) const noexcept
{
    int64_t local_x;
    int64_t local_z;
    uint32_t index;

    if (packed_light == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_implementation == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_implementation->phase != VOXEL_LIGHT_OPERATION_COMPLETE)
        return (FT_ERR_INVALID_STATE);
    if (world_y < 0 || world_y >= 256)
        return (FT_ERR_OUT_OF_RANGE);
    local_x = static_cast<int64_t>(world_x)
        - static_cast<int64_t>(this->_implementation->origin_x);
    local_z = static_cast<int64_t>(world_z)
        - static_cast<int64_t>(this->_implementation->origin_z);
    if (local_x < this->_implementation->region_min
        || local_x >= this->_implementation->region_max
        || local_z < this->_implementation->region_min
        || local_z >= this->_implementation->region_max)
        return (FT_ERR_OUT_OF_RANGE);
    index = this->_implementation->index(static_cast<int32_t>(local_x),
        world_y, static_cast<int32_t>(local_z));
    *packed_light = voxel_light_pack(this->_implementation->sky[index],
        this->_implementation->block[index]);
    return (FT_ERR_SUCCESS);
}

int32_t voxel_light_build_operation_lookup(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint8_t *packed_light) noexcept
{
    voxel_light_build_operation *operation;

    if (user_data == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    operation = static_cast<voxel_light_build_operation *>(user_data);
    return (operation->get_packed_light(world_x, world_y, world_z,
        packed_light));
}

static int32_t voxel_light_build_chunk_region(voxel_light_chunk &light_chunk,
    int32_t origin_x, int32_t origin_z,
    voxel_light_block_lookup_fn lookup, void *user_data,
    int32_t region_min, int32_t region_max,
    voxel_light_build_stats *stats) noexcept
{
    uint32_t index;
    int32_t error;

    if (lookup == ft_nullptr)
        return FT_ERR_INVALID_ARGUMENT;
    light_workspace *workspace = pt_thread_workspace<light_workspace>::get_or_create();
    if (workspace == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    std::vector<uint8_t> &sky = workspace->sky;
    std::vector<uint8_t> &block = workspace->block;
    std::vector<uint8_t> &sky_queued = workspace->sky_queued;
    std::vector<uint8_t> &block_queued = workspace->block_queued;
    std::vector<uint8_t> &step_costs = workspace->step_costs;
    std::vector<light_node> &queue = workspace->queue;
    const int32_t edge = region_max - region_min;
    const ft_size_t grid_size = static_cast<ft_size_t>(edge) * 256U * edge;
    try
    {
        sky.assign(grid_size, 0U);
        block.assign(grid_size, 0U);
        sky_queued.assign(grid_size, 0U);
        block_queued.assign(grid_size, 0U);
        step_costs.resize(grid_size);
    }
    catch (const std::bad_alloc &)
    {
        return (FT_ERR_NO_MEMORY);
    }
    queue.clear();
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
    uint16_t sky_floors[VOXEL_LIGHT_GRID_EDGE * VOXEL_LIGHT_GRID_EDGE];
    uint32_t column = 0U;
    for (int32_t z = region_min; z < region_max; ++z)
    {
        for (int32_t x = region_min; x < region_max; ++x)
        {
            ft_bool direct = FT_TRUE;
            uint16_t sky_floor = 256U;
			for (int32_t y = 255; y >= 0; --y)
			{
				uint32_t id = GAME_VOXEL_AIR_BLOCK;
				error = read_block(lookup, user_data, origin_x + x, y,
					origin_z + z, &id);
                if (error != FT_ERR_SUCCESS)
                    return error;
                if (stats != nullptr)
                    stats->scanned_cells += 1U;
                const voxel_block_metadata &metadata =
                    voxel_get_block_metadata(id);
                index = light_grid_index(x - region_min, y, z - region_min, edge);
                step_costs[index] = light_step_cost(metadata);
				if (direct == FT_TRUE && metadata.light_attenuation < 15U
					&& metadata.transparent == FT_TRUE)
				{
					sky[index] = 15U;
                    sky_floor = static_cast<uint16_t>(y);
                }
                else
                    direct = FT_FALSE;
                uint8_t emission = voxel_block_emitted_light_level(id);
                if (emission != 0U)
                {
                    if (emission > block[index])
                    {
                        block[index] = emission;
                        if (block_queued[index] == 0U)
                        {
                            error = light_queue_push(queue, {
                                static_cast<int16_t>(x),
                                static_cast<int16_t>(y),
                                static_cast<int16_t>(z), 1U, emission});
                            if (error != FT_ERR_SUCCESS)
                                return (error);
                            block_queued[index] = 1U;
                        }
                    }
                }
            }
            sky_floors[column] = sky_floor;
            column += 1U;
        }
    }
    // Cells at level 15 only contribute across the boundary of their columns.
    error = light_seed_sky_boundaries(*workspace, sky_floors, region_min, region_max);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (stats != nullptr)
        stats->queue_peak = queue.size();
    while (!queue.empty())
    {
        light_node node = queue.back();
        queue.pop_back();
        index = light_grid_index(node.x - region_min, node.y, node.z - region_min, edge);
        if (node.channel == 0U)
        {
            sky_queued[index] = 0U;
            node.level = sky[index];
        }
        else
        {
            block_queued[index] = 0U;
            node.level = block[index];
        }
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
            index = light_grid_index(x - region_min, y, z - region_min, edge);
            const uint8_t cost = step_costs[index];
            if (node.level <= cost)
                continue;
            uint8_t candidate = static_cast<uint8_t>(node.level - cost);
            uint8_t *destination = node.channel == 0U ? &sky[index] : &block[index];
            if (candidate <= *destination)
                continue;
            *destination = candidate;
            uint8_t *queued;
            if (node.channel == 0U)
                queued = &sky_queued[index];
            else
                queued = &block_queued[index];
            if (*queued == 0U)
            {
                error = light_queue_push(queue, {
                    static_cast<int16_t>(x), static_cast<int16_t>(y),
                    static_cast<int16_t>(z), node.channel, candidate});
                if (error != FT_ERR_SUCCESS)
                    return (error);
                *queued = 1U;
                if (stats != nullptr && queue.size() > stats->queue_peak)
                    stats->queue_peak = queue.size();
            }
        }
    }
    uint8_t packed[VOXEL_LIGHT_SECTION_BLOCKS];
    uint8_t section = 0U;
    while (section < GAME_VOXEL_CHUNK_SECTION_COUNT)
    {
        const int32_t first_y = section << 4U;
        const uint32_t first_index = light_grid_index(-region_min, first_y, -region_min, edge);
        const uint8_t first = static_cast<uint8_t>(sky[first_index] | (block[first_index] << 4U));
        uint8_t differences = 0U;
        uint16_t cell = 0U;
        int32_t y = first_y;
        while (y < first_y + 16)
        {
            int32_t z = 0;
            while (z < 16)
            {
                const uint32_t row = light_grid_index(-region_min, y, z - region_min, edge);
                uint32_t x = 0U;
                while (x < 16U)
                {
                    const uint8_t value = static_cast<uint8_t>(sky[row + x] | (block[row + x] << 4U));
                    packed[cell] = value;
                    differences |= value ^ first;
                    cell += 1U;
                    x += 1U;
                }
                z += 1;
            }
            y += 1;
        }
        error = light_store_section(light_chunk.get_section(section), packed, differences);
        if (error != FT_ERR_SUCCESS)
            return (error);
        section += 1U;
    }
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
