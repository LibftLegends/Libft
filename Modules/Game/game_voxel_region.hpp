#ifndef GAME_VOXEL_REGION_HPP
# define GAME_VOXEL_REGION_HPP

#include "game_voxel_chunk.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Geometry/geometry.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include <stdint.h>

#define GAME_VOXEL_REGION_CHUNKS_X 32
#define GAME_VOXEL_REGION_CHUNKS_Z 32
#define GAME_VOXEL_REGION_CHUNK_COUNT 1024
#define GAME_VOXEL_REGION_BLOCK_WIDTH 512

class game_voxel_region
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        game_voxel_chunk *_chunks[GAME_VOXEL_REGION_CHUNK_COUNT];
        ft_string   _storage_path;
        int32_t     _region_start_x;
        int32_t     _region_start_z;
        ft_bool     _dirty;
        uint8_t     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        static uint16_t chunk_slot_from_local(int32_t local_chunk_x,
            int32_t local_chunk_z) noexcept;
        int32_t build_file_path(ft_string &file_path) const noexcept;
        int32_t ensure_chunk(uint16_t slot,
            game_voxel_chunk **chunk_out) noexcept;
        int32_t clear_chunks() noexcept;

    public:
        game_voxel_region() noexcept;
        game_voxel_region(const game_voxel_region &other) noexcept = delete;
        game_voxel_region(game_voxel_region &&other) noexcept = delete;
        ~game_voxel_region() noexcept;

        game_voxel_region &operator=(const game_voxel_region &other) noexcept = delete;
        game_voxel_region &operator=(game_voxel_region &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t world_x, int32_t world_z,
            const char *storage_path) noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_voxel_region &other) noexcept;
        int32_t set_region_storage_path(const char *storage_path) noexcept;
        int32_t load_region(int32_t world_x, int32_t world_z) noexcept;
        int32_t save_region() noexcept;
        int32_t load_chunk(int32_t world_chunk_x, int32_t world_chunk_z,
            game_voxel_chunk **chunk_out) noexcept;
        int32_t save_chunk(int32_t world_chunk_x, int32_t world_chunk_z,
            const game_voxel_chunk &chunk) noexcept;
        int32_t read_block(int32_t world_x, int32_t world_y, int32_t world_z,
            uint32_t *block_id) const noexcept;
        int32_t write_block(int32_t world_x, int32_t world_y, int32_t world_z,
            uint32_t block_id) noexcept;
        ft_bool has_chunk(int32_t world_chunk_x,
            int32_t world_chunk_z) const noexcept;
        ft_bool is_chunk_loaded(int32_t world_chunk_x,
            int32_t world_chunk_z) const noexcept;
        ft_bool is_chunk_visible(int32_t world_chunk_x,
            int32_t world_chunk_z, const geometry_frustum &frustum) const noexcept;
        int32_t get_region_start_x() const noexcept;
        int32_t get_region_start_z() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

int32_t game_voxel_region_start_coordinate(int32_t coordinate) noexcept;
int32_t game_voxel_region_file_name_from_start(int32_t region_start_x,
    int32_t region_start_z, ft_string &file_name) noexcept;
int32_t game_voxel_region_file_name(int32_t world_x, int32_t world_z,
    ft_string &file_name) noexcept;

#endif
