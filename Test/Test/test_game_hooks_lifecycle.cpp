#include "../test_internal.hpp"

#include "../../Modules/Game/game_hooks.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_hooks_thread_safe_lifecycle)
{
    game_hooks hooks;
    ft_vector<ft_game_hook_metadata> *catalog_metadata;
    ft_function<void(game_character&, game_item&)> item_crafted_callback(
        [](game_character &, game_item &) noexcept
        {
            return ;
        });
    ft_function<void(game_character&, int32_t, uint8_t)> character_damaged_callback(
        [](game_character &, int32_t, uint8_t) noexcept
        {
            return ;
        });
    ft_function<void(game_world&, game_event&)> event_triggered_callback(
        [](game_world &, game_event &) noexcept
        {
            return ;
        });

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT(hooks.get_on_item_crafted() == FT_FALSE);
    FT_ASSERT(hooks.get_on_character_damaged() == FT_FALSE);
    FT_ASSERT(hooks.get_on_event_triggered() == FT_FALSE);
    hooks.set_on_item_crafted(ft_move(item_crafted_callback));
    hooks.set_on_character_damaged(ft_move(character_damaged_callback));
    hooks.set_on_event_triggered(ft_move(event_triggered_callback));
    FT_ASSERT(hooks.get_on_item_crafted() == FT_TRUE);
    FT_ASSERT(hooks.get_on_character_damaged() == FT_TRUE);
    FT_ASSERT(hooks.get_on_event_triggered() == FT_TRUE);
    catalog_metadata = hooks.get_catalog_metadata();
    FT_ASSERT(catalog_metadata != ft_nullptr);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), catalog_metadata->size());
    (void)catalog_metadata->destroy();
    delete catalog_metadata;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.enable_thread_safety());
    FT_ASSERT(hooks.is_thread_safe() == FT_TRUE);
    hooks.reset();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.destroy());
    return (1);
}
