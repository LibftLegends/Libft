#include "../test_internal.hpp"
#include "../../Modules/Game/game_hooks.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int32_t initialize_hook_metadata(ft_game_hook_metadata &metadata,
    const char *hook_identifier, const char *listener_name,
    const char *description, const char *argument_contract)
{
    int32_t error_code;

    error_code = metadata.hook_identifier.initialize(hook_identifier);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.listener_name.initialize(listener_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.description.initialize(description);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.argument_contract.initialize(argument_contract);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

static void destroy_metadata_vector(ft_vector<ft_game_hook_metadata> *metadata)
{
    if (metadata == ft_nullptr)
        return ;
    metadata->destroy();
    delete metadata;
    return ;
}

FT_TEST(test_game_hooks_getters_clone_callbacks)
{
    game_hooks hooks;
    game_character character;
    game_item item;
    int crafted_invocations;
    ft_game_hook_metadata metadata;

    crafted_invocations = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    hooks.set_on_item_crafted(ft_function<void(game_character&, game_item&)>([&crafted_invocations](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        crafted_invocations = crafted_invocations + 1;
        return ;
    }));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
        ft_game_hook_item_crafted_identifier, "getter.custom",
        "Metadata test listener", "game_character&,game_item&"));
    hooks.register_listener(metadata, 900, ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)>([](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        return ;
    })));
    ft_function<void(game_character&, game_item&)> callback_copy;

    callback_copy = hooks.get_on_item_crafted();
    FT_ASSERT(static_cast<bool>(callback_copy));
    callback_copy(character, item);
    hooks.invoke_on_item_crafted(character, item);
    FT_ASSERT_EQ(2, crafted_invocations);
    ft_vector<ft_game_hook_metadata> *catalog_after = hooks.get_catalog_metadata();
    ft_string identifier_string;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, identifier_string.initialize(ft_game_hook_item_crafted_identifier));
    ft_vector<ft_game_hook_metadata> *scoped_catalog_after =
        hooks.get_catalog_metadata_for(identifier_string);
    FT_ASSERT(catalog_after != ft_nullptr);
    FT_ASSERT(scoped_catalog_after != ft_nullptr);
    FT_ASSERT_EQ(2, catalog_after->size());
    FT_ASSERT_EQ(2, scoped_catalog_after->size());
    ft_string listener_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, listener_name.initialize("getter.custom"));
    hooks.unregister_listener(identifier_string, listener_name);
    ft_vector<ft_game_hook_metadata> *scoped_after =
        hooks.get_catalog_metadata_for(identifier_string);
    FT_ASSERT(scoped_after != ft_nullptr);
    FT_ASSERT_EQ(1, scoped_after->size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    destroy_metadata_vector(catalog_after);
    destroy_metadata_vector(scoped_catalog_after);
    destroy_metadata_vector(scoped_after);
    return (1);
}

FT_TEST(test_game_hooks_filtered_catalog_preserves_registration_order)
{
    game_hooks hooks;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.enable_thread_safety());
    char name[] = "listener-a";
    while (name[9] <= 'l')
    {
        const char *identifier = "target";
        if (name[9] == 'f' || name[9] == 'l')
            identifier = "other";
        ft_game_hook_metadata metadata;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, metadata.hook_identifier.initialize(identifier));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, metadata.listener_name.initialize(name));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, metadata.description.initialize("Description"));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, metadata.argument_contract.initialize("No arguments"));
        hooks.register_listener(metadata, name[9] - 'a', [](ft_game_hook_context &) {});
        FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
        ++name[9];
    }
    ft_string key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("target"));
    auto *matches = hooks.get_catalog_metadata_for(key);
    FT_ASSERT(matches != ft_nullptr);
    FT_ASSERT_EQ(10U, matches->size());
    FT_ASSERT((*matches)[0].listener_name == "listener-a");
    FT_ASSERT((*matches)[5].listener_name == "listener-g");
    FT_ASSERT((*matches)[9].listener_name == "listener-k");
    destroy_metadata_vector(matches);

    ft_string missing_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_key.initialize("missing"));
    auto *missing = hooks.get_catalog_metadata_for(missing_key);
    FT_ASSERT(missing != ft_nullptr);
    FT_ASSERT_EQ(0U, missing->size());
    destroy_metadata_vector(missing);
    return (1);
}
