#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/DUMB/sound_device.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_dumb_sound_clip_move_reuses_destination_spec_and_transfers_samples)
{
    ft_sound_clip source;
    ft_sound_clip destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    source._data.assign(64, 0x2BU);
    source._spec->freq = 22050;
    destination._data.assign(1024, 0x11U);
    const uint8_t *source_data = source.get_data();
    const ft_sound_spec *destination_spec = destination.get_spec();

    cma_set_alloc_limit(1);
    const int32_t status = destination.initialize(static_cast<ft_sound_clip &&>(source));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT(destination.get_data() == source_data);
    FT_ASSERT(destination.get_spec() == destination_spec);
    FT_ASSERT_EQ(64U, destination.get_size());
    FT_ASSERT_EQ(0x2BU, destination.get_data()[63]);
    FT_ASSERT_EQ(22050, destination.get_spec()->freq);
    FT_ASSERT_EQ(0U, source.get_size());
    FT_ASSERT_EQ(0U, source._data.capacity());
    FT_ASSERT_EQ(44100, source.get_spec()->freq);
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(0U, destination._data.capacity());
    return (1);
}

FT_TEST(test_dumb_sound_clip_copy_allocation_failure_preserves_source)
{
    ft_sound_clip source;
    ft_sound_clip destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source._data.assign(1024, 0x71U);

    cma_set_alloc_limit(128);
    const int32_t status = destination.initialize(source);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, status);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(0U, destination._data.capacity());
    FT_ASSERT(destination._spec == ft_nullptr);
    FT_ASSERT_EQ(1024U, source.get_size());
    FT_ASSERT_EQ(0x71U, source.get_data()[1023]);
    return (1);
}
