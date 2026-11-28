#include "../test_internal.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_vendor_profile_initialize_and_setters)
{
    game_vendor_profile profile;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.initialize(3, 1.20, 0.88, 0.05));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    FT_ASSERT_EQ(3, profile.get_vendor_id());
    FT_ASSERT_DOUBLE_EQ(1.20, profile.get_buy_markup());
    FT_ASSERT_DOUBLE_EQ(0.88, profile.get_sell_multiplier());
    FT_ASSERT_DOUBLE_EQ(0.05, profile.get_tax_rate());

    profile.set_vendor_id(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    profile.set_buy_markup(1.35);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    profile.set_sell_multiplier(0.72);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    profile.set_tax_rate(0.10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());

    FT_ASSERT_EQ(5, profile.get_vendor_id());
    FT_ASSERT_DOUBLE_EQ(1.35, profile.get_buy_markup());
    FT_ASSERT_DOUBLE_EQ(0.72, profile.get_sell_multiplier());
    FT_ASSERT_DOUBLE_EQ(0.10, profile.get_tax_rate());
    return (1);
}

FT_TEST(test_vendor_profile_initialize_copy_matches_source)
{
    game_vendor_profile original;
    game_vendor_profile clone;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(8, 1.45, 0.65, 0.12));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.initialize(original));

    FT_ASSERT_EQ(8, clone.get_vendor_id());
    FT_ASSERT_DOUBLE_EQ(1.45, clone.get_buy_markup());
    FT_ASSERT_DOUBLE_EQ(0.65, clone.get_sell_multiplier());
    FT_ASSERT_DOUBLE_EQ(0.12, clone.get_tax_rate());
    return (1);
}

FT_TEST(test_vendor_profile_catalog_fetch_retains_destination_mutex)
{
    game_economy_table table;
    game_vendor_profile source;
    game_vendor_profile destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7, 1.25, 0.8, 0.05));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_vendor_profile(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_vendor_profile(7, destination));
    FT_ASSERT_EQ(7, destination.get_vendor_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    destination.set_tax_rate(0.10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    source.set_tax_rate(0.20);

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t register_error = table.register_vendor_profile(source);
    const int32_t fetch_error = table.fetch_vendor_profile(7, destination);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, register_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_error);
    FT_ASSERT_DOUBLE_EQ(0.10, destination.get_tax_rate());
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_vendor_profile(7, destination));
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_DOUBLE_EQ(1.25, destination.get_buy_markup());
    FT_ASSERT_DOUBLE_EQ(0.8, destination.get_sell_multiplier());
    FT_ASSERT_DOUBLE_EQ(0.05, destination.get_tax_rate());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_vendor_profile(7, destination));
    FT_ASSERT_EQ(7, destination.get_vendor_id());
    return (1);
}
