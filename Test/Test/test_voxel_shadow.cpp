#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/voxel_shadow.hpp"

struct voxel_shadow_test_context
{
    int32_t solid_y_near;
    int32_t solid_y_far;
    int32_t world_x;
    int32_t world_z;
};

static ft_bool voxel_shadow_test_lookup(void *user_data, int32_t world_x,
    int32_t world_y, int32_t world_z) noexcept
{
    voxel_shadow_test_context *context;

    context = static_cast<voxel_shadow_test_context *>(user_data);
    if (context == nullptr || world_x != context->world_x
        || world_z != context->world_z)
        return (FT_FALSE);
    if (world_y == context->solid_y_near
        || world_y == context->solid_y_far)
        return (FT_TRUE);
    return (FT_FALSE);
}

FT_TEST(test_voxel_shadow_receiver_uses_nearest_valid_surface)
{
    voxel_shadow_test_context context;
    int32_t receiver_y;

    context.solid_y_near = 17;
    context.solid_y_far = 12;
    context.world_x = 4;
    context.world_z = -2;
    receiver_y = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_shadow_find_receiver(4, 20, -2, 8,
        voxel_shadow_test_lookup, &context, &receiver_y));
    FT_ASSERT_EQ(17, receiver_y);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, voxel_shadow_find_receiver(4, 20, -2, 2,
        voxel_shadow_test_lookup, &context, &receiver_y));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_shadow_find_receiver(4, 20,
        -2, -1, voxel_shadow_test_lookup, &context, &receiver_y));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_shadow_find_receiver(4, 20,
        -2, 8, nullptr, &context, &receiver_y));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_shadow_find_receiver(4, 20,
        -2, 8, voxel_shadow_test_lookup, &context, nullptr));
    return (1);
}

FT_TEST(test_voxel_shadow_height_fade_is_bounded)
{
    FT_ASSERT_EQ(1.0,
        voxel_shadow_height_fade(18.0, 17, 8.0));
    FT_ASSERT_EQ(0.5,
        voxel_shadow_height_fade(22.0, 17, 8.0));
    FT_ASSERT_EQ(0.0,
        voxel_shadow_height_fade(26.0, 17, 8.0));
    FT_ASSERT_EQ(1.0,
        voxel_shadow_height_fade(17.0, 17, 8.0));
    FT_ASSERT_EQ(0.0,
        voxel_shadow_height_fade(18.0, 17, 0.0));
    return (1);
}

#endif
