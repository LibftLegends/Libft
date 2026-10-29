#ifndef VOXEL_LIGHTING_HPP
# define VOXEL_LIGHTING_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include <stdint.h>
# include "../Errno/errno.hpp"

/* One byte: low nibble is skylight, high nibble is block light. */
uint8_t voxel_light_pack(uint8_t sky_light, uint8_t block_light) noexcept;
uint8_t voxel_light_sky(uint8_t packed_light) noexcept;
uint8_t voxel_light_block(uint8_t packed_light) noexcept;
uint8_t voxel_light_combined(uint8_t packed_light,
    uint8_t sky_darkening = 0U) noexcept;

class voxel_light_section
{
    private:
        uint8_t _uniform_light;
        uint8_t *_lights;
        uint8_t _initialised_state;

    public:
        voxel_light_section() noexcept;
        voxel_light_section(const voxel_light_section &other) noexcept = delete;
        voxel_light_section(voxel_light_section &&other) noexcept = delete;
        ~voxel_light_section() noexcept;
        voxel_light_section &operator=(const voxel_light_section &other)
            noexcept = delete;
        voxel_light_section &operator=(voxel_light_section &&other)
            noexcept = delete;
        int32_t initialize(uint8_t value = 0U) noexcept;
        int32_t destroy() noexcept;
        int32_t move(voxel_light_section &other) noexcept;
        uint8_t get(uint16_t index) const noexcept;
        int32_t set(uint16_t index, uint8_t value) noexcept;
        int32_t fill(uint8_t value) noexcept;
        ft_bool is_uniform() const noexcept;
        uint8_t get_uniform() const noexcept;
};

class voxel_light_chunk
{
    private:
        voxel_light_section _sections[16];
        uint8_t _initialised_state;

    public:
        voxel_light_chunk() noexcept;
        voxel_light_chunk(const voxel_light_chunk &other) noexcept = delete;
        voxel_light_chunk(voxel_light_chunk &&other) noexcept = delete;
        ~voxel_light_chunk() noexcept;
        voxel_light_chunk &operator=(const voxel_light_chunk &other)
            noexcept = delete;
        voxel_light_chunk &operator=(voxel_light_chunk &&other)
            noexcept = delete;
        int32_t initialize(uint8_t value = 0U) noexcept;
        int32_t destroy() noexcept;
        int32_t move(voxel_light_chunk &other) noexcept;
        uint8_t get(int32_t local_x, int32_t local_y,
            int32_t local_z) const noexcept;
        int32_t set(int32_t local_x, int32_t local_y,
            int32_t local_z, uint8_t value) noexcept;
        voxel_light_section &get_section(uint8_t section_index) noexcept;
        const voxel_light_section &get_section(uint8_t section_index)
            const noexcept;
};

typedef int32_t (*voxel_light_block_lookup_fn)(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint32_t *block_id) noexcept;

/* The callback does not transfer ownership of the returned sample. The
 * caller owns user_data and must keep it valid for the duration of the
 * lookup. World coordinates are absolute coordinates supplied by the
 * consumer, not chunk-local coordinates. */
typedef int32_t (*voxel_light_packed_lookup_fn)(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint8_t *packed_light) noexcept;

struct voxel_light_build_stats
{
    uint64_t scanned_cells;
    uint64_t propagated_cells;
    uint64_t queue_peak;
};

struct voxel_light_update_config
{
    uint32_t min_nodes_per_frame;
    uint32_t target_nodes_per_frame;
    uint32_t max_nodes_per_frame;
    uint64_t time_budget_microseconds;
};

class voxel_light_build_operation
{
    private:
        struct implementation;
        uint8_t _initialised_state;
        implementation *_implementation;

    public:
        voxel_light_build_operation() noexcept;
        voxel_light_build_operation(const voxel_light_build_operation &other)
            noexcept = delete;
        voxel_light_build_operation(voxel_light_build_operation &&other)
            noexcept = delete;
        ~voxel_light_build_operation() noexcept;

        voxel_light_build_operation &operator=(
            const voxel_light_build_operation &other) noexcept = delete;
        voxel_light_build_operation &operator=(
            voxel_light_build_operation &&other) noexcept = delete;

        int32_t initialize(voxel_light_chunk &light_chunk,
            int32_t world_origin_x, int32_t world_origin_z,
            voxel_light_block_lookup_fn lookup_block, void *user_data,
            ft_bool include_halo) noexcept;
        int32_t destroy() noexcept;
        int32_t move(voxel_light_build_operation &other) noexcept;
        int32_t step(const voxel_light_update_config &config,
            voxel_light_build_stats *stats, ft_bool *complete) noexcept;
        ft_bool is_complete() const noexcept;
        /* The operation owns its internal light workspace. This query only
         * copies a completed sample into packed_light; it never exposes that
         * workspace. The operation, destination light chunk, callback data,
         * and any object passed as user_data to the build must remain alive
         * and externally synchronized for the operation's lifetime. A query
         * is valid only after completion and only for the computed target
         * region plus halo selected at initialize(). */
        int32_t get_packed_light(int32_t world_x, int32_t world_y,
            int32_t world_z, uint8_t *packed_light) const noexcept;
};

/* Adapter for passing a completed operation as a
 * voxel_light_packed_lookup_fn. user_data must point to a live operation. */
int32_t voxel_light_build_operation_lookup(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint8_t *packed_light) noexcept;

void voxel_light_update_config_defaults(voxel_light_update_config &config) noexcept;
ft_bool voxel_light_update_config_is_valid(
    const voxel_light_update_config &config) noexcept;

int32_t voxel_light_build_chunk(voxel_light_chunk &light_chunk,
    int32_t world_origin_x, int32_t world_origin_z,
    voxel_light_block_lookup_fn lookup_block, void *user_data,
    voxel_light_build_stats *stats = nullptr) noexcept;

/* Fast path for a newly generated chunk with no loaded neighbor sources. */
int32_t voxel_light_build_chunk_local(voxel_light_chunk &light_chunk,
    int32_t world_origin_x, int32_t world_origin_z,
    voxel_light_block_lookup_fn lookup_block, void *user_data,
    voxel_light_build_stats *stats = nullptr) noexcept;

#endif

#endif
