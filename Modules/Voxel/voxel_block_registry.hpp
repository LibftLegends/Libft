#ifndef VOXEL_BLOCK_REGISTRY_HPP
# define VOXEL_BLOCK_REGISTRY_HPP

# include "terrain_types.hpp"
# include "../CPP_class/class_string.hpp"
# include "../Buffer/byte_buffer.hpp"
# include "../Template/shared_ptr.hpp"

struct terrain_runtime_block
{
    uint32_t block_id;
    terrain_block_metadata metadata;
    ft_string name;
    ft_string asset_paths[TERRAIN_BLOCK_ASSET_FACE_COUNT];
    ft_byte_buffer asset_data[TERRAIN_BLOCK_ASSET_FACE_COUNT];

    terrain_runtime_block() noexcept;
    ~terrain_runtime_block() noexcept;
};

class terrain_runtime_block_handle
{
    friend int32_t terrain_acquire_block(uint32_t block_id,
        terrain_runtime_block_handle &handle) noexcept;

    private:
        ft_sharedptr<terrain_runtime_block> _block;
        uint8_t _initialised_state;

    public:
        terrain_runtime_block_handle() noexcept;
        terrain_runtime_block_handle(
            const terrain_runtime_block_handle &other) noexcept = delete;
        terrain_runtime_block_handle(terrain_runtime_block_handle &&other)
            noexcept = delete;
        ~terrain_runtime_block_handle() noexcept;

        terrain_runtime_block_handle &operator=(
            const terrain_runtime_block_handle &other) noexcept = delete;
        terrain_runtime_block_handle &operator=(
            terrain_runtime_block_handle &&other) noexcept = delete;

        int32_t initialize(
            const terrain_runtime_block_handle &other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(terrain_runtime_block_handle &other) noexcept;
        ft_bool is_valid() const noexcept;
        uint32_t get_id() const noexcept;
        const terrain_block_metadata *get_metadata() const noexcept;
        const char *get_name() const noexcept;
        const char *get_asset_path(terrain_block_asset_face face) const noexcept;
        const uint8_t *get_asset_data(terrain_block_asset_face face,
            ft_size_t *size_out) const noexcept;
};

const terrain_block_metadata *terrain_runtime_find_block_metadata(
    uint32_t block_id) noexcept;
ft_bool terrain_runtime_block_is_known(uint32_t block_id) noexcept;
int32_t terrain_acquire_block(uint32_t block_id,
    terrain_runtime_block_handle &handle) noexcept;

#ifdef LIBFT_TEST_BUILD
void terrain_runtime_reset_for_tests(void) noexcept;
#endif

#endif
