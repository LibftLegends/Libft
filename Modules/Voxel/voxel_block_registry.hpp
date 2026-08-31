#ifndef VOXEL_BLOCK_REGISTRY_HPP
# define VOXEL_BLOCK_REGISTRY_HPP

# include "voxel_types.hpp"
# include "../CPP_class/class_string.hpp"
# include "../Buffer/byte_buffer.hpp"
# include "../Template/shared_ptr.hpp"

struct voxel_runtime_block
{
    uint32_t block_id;
    voxel_block_metadata metadata;
    ft_string name;
    ft_string asset_paths[VOXEL_BLOCK_ASSET_FACE_COUNT];
    ft_byte_buffer asset_data[VOXEL_BLOCK_ASSET_FACE_COUNT];

    voxel_runtime_block() noexcept;
    ~voxel_runtime_block() noexcept;
};

class voxel_runtime_block_handle
{
    friend int32_t voxel_acquire_block(uint32_t block_id,
        voxel_runtime_block_handle &handle) noexcept;

    private:
        ft_sharedptr<voxel_runtime_block> _block;
        uint8_t _initialised_state;

    public:
        voxel_runtime_block_handle() noexcept;
        voxel_runtime_block_handle(
            const voxel_runtime_block_handle &other) noexcept = delete;
        voxel_runtime_block_handle(voxel_runtime_block_handle &&other)
            noexcept = delete;
        ~voxel_runtime_block_handle() noexcept;

        voxel_runtime_block_handle &operator=(
            const voxel_runtime_block_handle &other) noexcept = delete;
        voxel_runtime_block_handle &operator=(
            voxel_runtime_block_handle &&other) noexcept = delete;

        int32_t initialize(
            const voxel_runtime_block_handle &other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(voxel_runtime_block_handle &other) noexcept;
        ft_bool is_valid() const noexcept;
        uint32_t get_id() const noexcept;
        const voxel_block_metadata *get_metadata() const noexcept;
        const char *get_name() const noexcept;
        const char *get_asset_path(voxel_block_asset_face face) const noexcept;
        const uint8_t *get_asset_data(voxel_block_asset_face face,
            ft_size_t *size_out) const noexcept;
};

const voxel_block_metadata *voxel_runtime_find_block_metadata(
    uint32_t block_id) noexcept;
ft_bool voxel_runtime_block_is_known(uint32_t block_id) noexcept;
int32_t voxel_acquire_block(uint32_t block_id,
    voxel_runtime_block_handle &handle) noexcept;

#ifdef LIBFT_TEST_BUILD
void voxel_runtime_reset_for_tests(void) noexcept;
#endif

#endif
