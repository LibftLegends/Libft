#include "../test_internal.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_quest_storage_remains_reusable_after_copy_failure)
{
    game_quest source;
    game_quest destination;
    ft_string description;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, description.initialize("Reach the gate and return to camp"));
    source.set_description(description);
    source.set_id(14);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t copy_error = destination.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination.get_error());
    FT_ASSERT_EQ(14, source.get_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(14, destination.get_id());
    FT_ASSERT_EQ(description.size(), destination.get_description().size());
    return (1);
}

FT_TEST(test_game_quest_storage_moves_heap_rewards_and_description_without_allocation)
{
    game_quest source;
    game_quest destination;
    ft_string description;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, description.initialize("Return to camp with the supplies"));
    source.set_description(description);
    ft_sharedptr<game_item> reward(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reward->initialize());
    ft_size_t count = 0U;
    while (count < 12U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_reward_items().push_back(reward));
        ++count;
    }
    const auto *storage = source.get_reward_items().begin();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t moved = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(12U, destination.get_reward_items().size());
    FT_ASSERT(destination.get_reward_items().begin() == storage);
    FT_ASSERT_EQ(description.size(), destination.get_description().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_reward_items().size());
    return (1);
}

FT_TEST(test_game_quest_text_replacement_reuses_storage_and_preserves_failure)
{
    game_quest quest;
    ft_string original;
    ft_string shorter;
    ft_string longer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize("starting description"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter.initialize("new"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        longer.initialize("A longer description that exceeds the existing storage capacity"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter.enable_thread_safety());
    quest.set_description(original);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    quest.set_objective(original);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    quest.set_description(shorter);
    const int32_t shorter_error = quest.get_error();
    quest.set_objective(longer);
    const int32_t longer_error = quest.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, longer_error);
    FT_ASSERT_EQ(shorter.size(), quest.get_description().size());
    FT_ASSERT_EQ(original.size(), quest.get_objective().size());

    quest.set_description(quest.get_description());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    quest.set_objective(longer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    FT_ASSERT_EQ(longer.size(), quest.get_objective().size());
    return (1);
}

FT_TEST(test_game_quest_reward_replacement_copies_live_items_transactionally)
{
    game_quest quest;
    ft_vector<ft_sharedptr<game_item> > incoming;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, incoming.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_reward_items().enable_thread_safety());
    ft_sharedptr<game_item> original(new game_item());
    ft_sharedptr<game_item> replacement(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_reward_items().push_back(original));
    incoming.reserve(128U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, incoming.get_error());
    ft_size_t index = 0U;
    while (index < 12U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, incoming.push_back(replacement));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    quest.set_reward_items(incoming);
    const int32_t error = quest.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(1U, quest.get_reward_items().size());
    FT_ASSERT(quest.get_reward_items()[0].get() == original.get());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(12U * sizeof(ft_sharedptr<game_item>)));
    quest.set_reward_items(incoming);
    const int32_t replaced = quest.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replaced);
    FT_ASSERT_EQ(FT_TRUE, quest.get_reward_items().is_thread_safe());
    FT_ASSERT_EQ(12U, quest.get_reward_items().size());
    FT_ASSERT_EQ(12U, quest.get_reward_items().capacity());
    FT_ASSERT(quest.get_reward_items()[11].get() == replacement.get());
    return (1);
}
