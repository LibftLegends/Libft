#include "../test_internal.hpp"

#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_equipment_thread_safe_lifecycle)
{
    game_equipment equipment;
    ft_sharedptr<game_item> *head_item;
    ft_sharedptr<game_item> *chest_item;
    ft_sharedptr<game_item> *weapon_item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.initialize());
    head_item = equipment.get_item(EQUIP_HEAD);
    chest_item = equipment.get_item(EQUIP_CHEST);
    weapon_item = equipment.get_item(EQUIP_WEAPON);
    FT_ASSERT(head_item != ft_nullptr);
    FT_ASSERT(chest_item != ft_nullptr);
    FT_ASSERT(weapon_item != ft_nullptr);
    FT_ASSERT(head_item->get() == ft_nullptr);
    FT_ASSERT(chest_item->get() == ft_nullptr);
    FT_ASSERT(weapon_item->get() == ft_nullptr);
    (void)head_item->destroy();
    (void)chest_item->destroy();
    (void)weapon_item->destroy();
    delete head_item;
    delete chest_item;
    delete weapon_item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.enable_thread_safety());
    FT_ASSERT(equipment.is_thread_safe() == FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.destroy());
    return (1);
}
