#ifndef VOXEL_GENERATION_HPP
# define VOXEL_GENERATION_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "voxel_config.hpp"
# include "../CPP_class/class_big_number.hpp"

class voxel_generation_context
{
    private:
        voxel_generation_config _config;
        uint32_t _configuration_signature;
        uint8_t _initialised_state;

    public:
        voxel_generation_context() noexcept;
        voxel_generation_context(
            const voxel_generation_context &other) noexcept = delete;
        voxel_generation_context(voxel_generation_context &&other)
            noexcept = delete;
        ~voxel_generation_context() noexcept;

        voxel_generation_context &operator=(
            const voxel_generation_context &other) noexcept = delete;
        voxel_generation_context &operator=(
            voxel_generation_context &&other) noexcept = delete;

        int32_t initialize(const voxel_generation_config &config) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(voxel_generation_context &other) noexcept;
        ft_bool is_initialised() const noexcept;
        const voxel_generation_config &config() const noexcept;
        uint32_t configuration_signature() const noexcept;
};

class voxel_world_chunk_coordinate
{
    private:
        ft_big_number _chunk_x;
        ft_big_number _chunk_z;
        uint8_t _initialised_state;

    public:
        voxel_world_chunk_coordinate() noexcept;
        voxel_world_chunk_coordinate(
            const voxel_world_chunk_coordinate &other) noexcept = delete;
        voxel_world_chunk_coordinate(
            voxel_world_chunk_coordinate &&other) noexcept = delete;
        ~voxel_world_chunk_coordinate() noexcept;
        voxel_world_chunk_coordinate &operator=(
            const voxel_world_chunk_coordinate &other) noexcept = delete;
        voxel_world_chunk_coordinate &operator=(
            voxel_world_chunk_coordinate &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(
            const voxel_world_chunk_coordinate &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(voxel_world_chunk_coordinate &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_chunk_coordinates(const char *chunk_x,
            const char *chunk_z) noexcept;
        const ft_big_number &chunk_x() const noexcept;
        const ft_big_number &chunk_z() const noexcept;
        uint64_t hash() const noexcept;
};

#endif
#endif
