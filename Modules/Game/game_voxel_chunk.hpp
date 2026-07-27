#ifndef GAME_VOXEL_CHUNK_HPP
# define GAME_VOXEL_CHUNK_HPP

#include "../Buffer/byte_buffer.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
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

class game_voxel_chunk_section
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint32_t    _uniform_block_id;
        uint32_t    *_palette;
        uint8_t     *_indices;
        uint16_t    _palette_size;
        uint8_t     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t materialize(uint32_t second_block_id) noexcept;
        int32_t find_or_add_palette_index(uint32_t block_id,
            uint8_t *palette_index) noexcept;
        void collapse_if_uniform() noexcept;

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
        uint8_t     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        static uint16_t local_index(int32_t local_x, int32_t local_y,
            int32_t local_z) noexcept;

    public:
        game_voxel_chunk() noexcept;
        game_voxel_chunk(const game_voxel_chunk &other) noexcept = delete;
        game_voxel_chunk(game_voxel_chunk &&other) noexcept = delete;
        ~game_voxel_chunk() noexcept;

        game_voxel_chunk &operator=(const game_voxel_chunk &other) noexcept = delete;
        game_voxel_chunk &operator=(game_voxel_chunk &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_voxel_chunk &other) noexcept;
        int32_t read_block(int32_t local_x, int32_t local_y, int32_t local_z,
            uint32_t *block_id) const noexcept;
        int32_t write_block(int32_t local_x, int32_t local_y, int32_t local_z,
            uint32_t block_id) noexcept;
        int32_t write_generated_block(int32_t local_x, int32_t local_y,
            int32_t local_z, uint32_t block_id) noexcept;
        ft_bool is_dirty() const noexcept;
        void clear_dirty() noexcept;
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
        int32_t serialize(ft_byte_buffer &buffer) const noexcept;
        int32_t deserialize(ft_byte_buffer &buffer) noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
