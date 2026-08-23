#ifndef TERRAIN_GENERATION_HPP
# define TERRAIN_GENERATION_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "terrain_config.hpp"
# include "../CPP_class/class_big_number.hpp"

class terrain_generation_context
{
    private:
        terrain_generation_config _config;
        uint32_t _configuration_signature;
        uint8_t _initialised_state;

    public:
        terrain_generation_context() noexcept;
        terrain_generation_context(
            const terrain_generation_context &other) noexcept = delete;
        terrain_generation_context(terrain_generation_context &&other)
            noexcept = delete;
        ~terrain_generation_context() noexcept;

        terrain_generation_context &operator=(
            const terrain_generation_context &other) noexcept = delete;
        terrain_generation_context &operator=(
            terrain_generation_context &&other) noexcept = delete;

        int32_t initialize(const terrain_generation_config &config) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(terrain_generation_context &other) noexcept;
        ft_bool is_initialised() const noexcept;
        const terrain_generation_config &config() const noexcept;
        uint32_t configuration_signature() const noexcept;
};

class terrain_world_chunk_coordinate
{
    private:
        ft_big_number _chunk_x;
        ft_big_number _chunk_z;
        uint8_t _initialised_state;

    public:
        terrain_world_chunk_coordinate() noexcept;
        terrain_world_chunk_coordinate(
            const terrain_world_chunk_coordinate &other) noexcept = delete;
        terrain_world_chunk_coordinate(
            terrain_world_chunk_coordinate &&other) noexcept = delete;
        ~terrain_world_chunk_coordinate() noexcept;
        terrain_world_chunk_coordinate &operator=(
            const terrain_world_chunk_coordinate &other) noexcept = delete;
        terrain_world_chunk_coordinate &operator=(
            terrain_world_chunk_coordinate &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(
            const terrain_world_chunk_coordinate &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_world_chunk_coordinate &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_chunk_coordinates(const char *chunk_x,
            const char *chunk_z) noexcept;
        const ft_big_number &chunk_x() const noexcept;
        const ft_big_number &chunk_z() const noexcept;
        uint64_t hash() const noexcept;
};

#endif

#endif
