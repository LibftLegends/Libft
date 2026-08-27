#include "../../Modules/Game/game_data_catalog.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void game_item_definition_get_weight(game_item_definition &value)
{
    (void)value.get_weight();
    return ;
}
static void game_item_definition_set_weight(game_item_definition &value)
{
    value.set_weight(1);
    return ;
}
static void game_item_definition_get_slot(game_item_definition &value)
{
    (void)value.get_slot_requirement();
    return ;
}
static void game_item_definition_set_slot(game_item_definition &value)
{
    value.set_slot_requirement(1);
    return ;
}
static void game_item_definition_get_error(game_item_definition &value)
{
    (void)value.get_error();
    return ;
}
static void game_item_definition_get_error_str(game_item_definition &value)
{
    (void)value.get_error_str();
    return ;
}
static void game_item_definition_enable(game_item_definition &value)
{
    (void)value.enable_thread_safety();
    return ;
}
static void game_item_definition_is_thread_safe(game_item_definition &value)
{
    (void)value.is_thread_safe();
    return ;
}
static void game_item_definition_destroy(game_item_definition &value)
{
    (void)value.destroy();
    return ;
}
static void game_item_definition_move(game_item_definition &value)
{
    (void)value.move(value);
    return ;
}

FT_TEST(test_game_item_definition_get_weight_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_get_weight));
    return (1);
}
FT_TEST(test_game_item_definition_set_weight_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_set_weight));
    return (1);
}
FT_TEST(test_game_item_definition_get_slot_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_get_slot));
    return (1);
}
FT_TEST(test_game_item_definition_set_slot_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_set_slot));
    return (1);
}
FT_TEST(test_game_item_definition_get_error_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_get_error));
    return (1);
}
FT_TEST(test_game_item_definition_get_error_str_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_get_error_str));
    return (1);
}
FT_TEST(test_game_item_definition_enable_aborts)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_enable));
    return (1);
}
FT_TEST(test_game_item_definition_is_thread_safe_aborts)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_is_thread_safe));
    return (1);
}
FT_TEST(test_game_item_definition_destroy_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_destroy));
    return (1);
}
FT_TEST(test_game_item_definition_move_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_item_definition>(
                        game_item_definition_move));
    return (1);
}
