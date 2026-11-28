#ifndef GAME_VOXEL_CHUNK_HPP
# define GAME_VOXEL_CHUNK_HPP

#include "../Buffer/byte_buffer.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "game_block_edit_op.hpp"
#include "game_world_delta.hpp"
#include "../PThread/pthread.hpp"
#include <stdint.h>

#define GAME_VOXEL_CHUNK_WIDTH 16
#define GAME_VOXEL_CHUNK_DEPTH 16
#define GAME_VOXEL_CHUNK_HEIGHT 256
#define GAME_VOXEL_SECTION_EDGE 16
#define GAME_VOXEL_SECTION_BLOCKS 4096
#define GAME_VOXEL_CHUNK_SECTION_COUNT 16
#define GAME_VOXEL_AIR_BLOCK 0U

struct game_voxel_generation_metadata
{
    uint64_t seed_value;
    int32_t world_block_origin_x;
    int32_t world_block_origin_z;
    uint32_t configuration_signature;
    uint32_t completed_stage_mask;
    uint32_t generator_version;
    ft_bool valid;
};

struct game_voxel_block_override
{
    uint16_t local_index;
    uint32_t generated_block_id;
    uint32_t current_block_id;
};

struct game_voxel_generated_column_block
{
    int32_t local_y;
    uint32_t block_id;
};

struct game_voxel_generated_block
{
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
};

struct game_voxel_generated_column_check
{
    int32_t first_y;
    int32_t last_y;
    // Called under the chunk write lock. The predicate must not re-enter the chunk.
    ft_bool (*accepts)(int32_t local_y, uint32_t block_id, const void *user_data) noexcept;
    const void *user_data;
};

class game_voxel_chunk_section
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint32_t    _uniform_block_id;
        // The palette owns the allocation and the indices follow it in storage.
        uint32_t    *_palette;
        uint8_t     *_indices;
        uint16_t    _palette_size;
        uint8_t     _initialised_state;
        uint8_t     _uniform_candidate;
        uint16_t    _uniform_candidate_count;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t allocate_payload() noexcept;
        int32_t find_or_add_palette_index(uint32_t block_id,
            uint8_t *palette_index) noexcept;
        void collapse_to_candidate() noexcept;
        int32_t copy_payload(const game_voxel_chunk_section &other) noexcept;
        void copy_rows(uint32_t *output, uint32_t first_index,
            uint32_t width, uint32_t block_stride) const noexcept;
        template <typename Reader>
        int32_t deserialize_from(Reader &buffer) noexcept;

        friend class game_voxel_chunk;

    public:
        game_voxel_chunk_section() noexcept;
        game_voxel_chunk_section(const game_voxel_chunk_section &other) noexcept
            = delete;
        game_voxel_chunk_section(game_voxel_chunk_section &&other) noexcept
            = delete;
        ~game_voxel_chunk_section() noexcept;

        game_voxel_chunk_section &operator=(
            const game_voxel_chunk_section &other) noexcept = delete;
        game_voxel_chunk_section &operator=(
            game_voxel_chunk_section &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_voxel_chunk_section &other) noexcept;
        uint32_t get_block(uint16_t local_index) const noexcept;
        int32_t set_block(uint16_t local_index, uint32_t block_id) noexcept;
        ft_bool is_uniform() const noexcept;
        ft_bool is_materialized() const noexcept;
        uint32_t get_uniform_block() const noexcept;
        uint16_t get_palette_size() const noexcept;
        uint32_t get_palette_block(uint8_t palette_index) const noexcept;
        int32_t serialize(ft_byte_buffer &buffer) const noexcept;
        int32_t deserialize(ft_byte_buffer &buffer) noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

class game_voxel_chunk
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        game_voxel_chunk_section _sections[GAME_VOXEL_CHUNK_SECTION_COUNT];
        ft_bool     _dirty;
        ft_bool     _generation_protected;
        game_voxel_generation_metadata _generation_metadata;
        uint32_t    _biome_id;
        game_voxel_block_override *_player_overrides;
        uint32_t    _player_override_count;
        uint32_t    _player_override_capacity;
        uint64_t    _revision;
        ft_bool     _last_request_valid;
        game_block_change_request _last_request;
        game_block_delta _last_delta;
        uint8_t     _initialised_state;
        mutable t_pt_rwlock *_access_lock;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        static uint16_t local_index(int32_t local_x, int32_t local_y,
            int32_t local_z) noexcept;
        int32_t grow_player_overrides(uint32_t minimum_capacity) noexcept;
        uint32_t player_override_first() const noexcept;
        void compact_player_overrides(uint32_t first, ft_bool rebuild_index) noexcept;
        uint32_t *player_override_index_table() const noexcept;
        uint32_t player_override_bucket(const uint32_t *table, uint16_t coordinate,
            uint32_t mask) const noexcept;
        void rebuild_player_override_index() noexcept;
        void index_player_override(uint32_t index) noexcept;
        void remove_player_override_from_index(uint32_t *table, uint32_t index) noexcept;
        void shift_player_override_index(uint32_t *table, uint32_t first,
            uint32_t end, ft_bool forward) noexcept;
        int32_t find_player_override(uint16_t local_index,
            uint32_t *override_index) const noexcept;
        void remove_player_override(uint32_t override_index) noexcept;
        friend class game_voxel_region;
        friend class game_world_delta_channel;
        friend int32_t game_world_delta_snapshot_serialize(const game_voxel_chunk &chunk,
            ft_byte_buffer &buffer) noexcept;
        friend int32_t game_world_delta_snapshot_deserialize(game_voxel_chunk &chunk,
            ft_byte_buffer &buffer) noexcept;
        int32_t serialize_locked(ft_byte_buffer &buffer) const noexcept;
        int32_t serialize_internal(ft_byte_buffer &buffer) const noexcept;
        int32_t deserialize_view(const uint8_t *data, ft_size_t size) noexcept;
        template <typename Reader>
        int32_t deserialize_from(Reader &buffer) noexcept;
        template <typename Reader>
        int32_t deserialize_internal(Reader &buffer) noexcept;
        int32_t read_block_locked(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t *block_id) const noexcept;
        int32_t copy_blocks_locked(uint32_t *blocks_out,
            uint32_t block_count) const noexcept;
        int32_t copy_region_locked(uint32_t *blocks_out,
            uint32_t block_count, uint32_t first_x, uint32_t first_z,
            uint32_t region_width, uint32_t region_depth) const noexcept;
        int32_t copy_x_border_locked(uint32_t *blocks_out,
            uint32_t block_count, uint32_t local_x) const noexcept;
        int32_t copy_z_border_locked(uint32_t *blocks_out,
            uint32_t block_count, uint32_t local_z) const noexcept;
        struct block_edit_state
        {
            uint32_t current_block_id;
            uint32_t generated_block_id;
            uint32_t override_index;
        };
        block_edit_state inspect_block_edit(uint16_t chunk_index,
            uint32_t current_block_id) const noexcept;
        int32_t apply_block_edit_at(uint16_t chunk_index, uint32_t block_id,
            const block_edit_state &state) noexcept;
        int32_t apply_block_edit_locked(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t block_id) noexcept;
        int32_t apply_authoritative_block_change_with_revision(
            const game_block_change_request &request, game_block_delta *delta_out,
            uint64_t *revision_before) noexcept;
        int32_t apply_authoritative_block_change_locked(
            const game_block_change_request &request,
            game_block_delta *delta_out) noexcept;
        int32_t apply_authoritative_block_delta_locked(
            const game_block_delta &delta) noexcept;
        int32_t write_generated_block_locked(int32_t local_x,
            int32_t local_y, int32_t local_z, uint32_t block_id) noexcept;
        int32_t write_generated_block_at(uint16_t chunk_index, uint32_t block_id) noexcept;
        int32_t clear_generated_blocks_locked() noexcept;
        int32_t get_generated_block_locked(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t *block_id) const noexcept;
        ft_bool is_block_player_modified_locked(int32_t local_x,
            int32_t local_y, int32_t local_z) const noexcept;
        void clear_dirty_locked() noexcept;
        void clear_generation_metadata_locked() noexcept;
        int32_t destroy_locked() noexcept;
        int32_t move_payload_locked(game_voxel_chunk &other) noexcept;

    public:
        game_voxel_chunk() noexcept;
        game_voxel_chunk(const game_voxel_chunk &other) noexcept = delete;
        game_voxel_chunk(game_voxel_chunk &&other) noexcept = delete;
        ~game_voxel_chunk() noexcept;

        game_voxel_chunk &operator=(const game_voxel_chunk &other) noexcept = delete;
        game_voxel_chunk &operator=(game_voxel_chunk &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_voxel_chunk &other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_voxel_chunk &other) noexcept;
        int32_t read_block(int32_t local_x, int32_t local_y, int32_t local_z,
            uint32_t *block_id) const noexcept;
        int32_t copy_blocks(uint32_t *blocks_out,
            uint32_t block_count) const noexcept;
        int32_t copy_region(uint32_t *blocks_out, uint32_t block_count,
            uint32_t first_x, uint32_t first_z, uint32_t region_width,
            uint32_t region_depth) const noexcept;
        int32_t copy_x_border(uint32_t *blocks_out,
            uint32_t block_count, uint32_t local_x) const noexcept;
        int32_t copy_z_border(uint32_t *blocks_out,
            uint32_t block_count, uint32_t local_z) const noexcept;
        int32_t write_block(int32_t local_x, int32_t local_y, int32_t local_z,
            uint32_t block_id) noexcept;
        int32_t apply_block_edit(int32_t local_x, int32_t local_y,
            int32_t local_z, const game_block_edit_op &edit) noexcept;
        int32_t apply_authoritative_block_change(
            const game_block_change_request &request,
            game_block_delta *delta_out) noexcept;
        int32_t apply_authoritative_block_delta(
            const game_block_delta &delta) noexcept;
        int32_t write_generated_block(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t block_id) noexcept;
        // Checks all destinations before writing in order under one write lock.
        // A rejected block skips the whole batch. The predicate must not re-enter the chunk.
        int32_t write_generated_blocks_if(const game_voxel_generated_block *blocks,
            uint32_t count, ft_bool (*accepts)(uint32_t block_id) noexcept) noexcept;
        // Validates coordinates first, then writes in order under one chunk lock.
        int32_t write_generated_column(int32_t local_x, int32_t local_z,
            const game_voxel_generated_column_block *blocks, uint32_t count) noexcept;
        int32_t write_generated_column(int32_t local_x, int32_t local_z,
            const game_voxel_generated_column_block *blocks, uint32_t count,
            const game_voxel_generated_column_check *check, ft_bool *applied) noexcept;
        int32_t clear_generated_blocks() noexcept;
        ft_bool is_dirty() const noexcept;
        void clear_dirty() noexcept;
        void clear_persistence_dirty() noexcept;
        void clear_generation_metadata() noexcept;
        ft_bool is_generation_protected() const noexcept;
        int32_t set_generation_metadata(
            const game_voxel_generation_metadata &metadata) noexcept;
        const game_voxel_generation_metadata &get_generation_metadata()
            const noexcept;
        ft_bool has_generation_metadata() const noexcept;
        ft_bool generation_metadata_matches(uint64_t seed_value,
            int32_t world_block_origin_x, int32_t world_block_origin_z,
            uint32_t configuration_signature) const noexcept;
        game_voxel_chunk_section &get_section(uint8_t section_index) noexcept;
        const game_voxel_chunk_section &get_section(
            uint8_t section_index) const noexcept;
        uint32_t get_biome_id() const noexcept;
        void set_biome_id(uint32_t biome_id) noexcept;
        int32_t record_dirty_edit(const game_block_edit_op &edit) noexcept;
        ft_bool is_block_player_modified(int32_t local_x, int32_t local_y,
            int32_t local_z) const noexcept;
        int32_t get_generated_block(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t *block_id) const noexcept;
        uint32_t get_player_override_count() const noexcept;
        int32_t get_player_override(uint32_t index,
            game_voxel_block_override *override_out) const noexcept;
        uint64_t get_revision() const noexcept;
        int32_t serialize(ft_byte_buffer &buffer) const noexcept;
        int32_t deserialize(ft_byte_buffer &buffer) noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
