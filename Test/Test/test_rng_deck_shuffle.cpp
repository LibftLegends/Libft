#include "../test_internal.hpp"
#include "../../Modules/RNG/deck.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <random>

FT_TEST(test_rng_deck_shuffle_preserves_seeded_order_across_batches)
{
    constexpr int32_t CARD_COUNT = 131;
    constexpr uint32_t SEED = 67179U;
    int32_t cards[CARD_COUNT];
    int32_t expected_order[CARD_COUNT];
    ft_deck<int32_t> deck;
    std::mt19937 reference(SEED);
    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.enable_thread_safety());
    int32_t index = 0;
    while (index < CARD_COUNT)
    {
        cards[index] = index;
        expected_order[index] = index;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.push_back(&cards[index]));
        index += 1;
    }
    index = CARD_COUNT - 1;
    while (index > 0)
    {
        ft_swap(expected_order[index], expected_order[distribution(reference) % (index + 1)]);
        index -= 1;
    }

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    ft_seed_random_engine(SEED);
    const int32_t shuffle_error = deck.shuffle();
    const int32_t next_draw = ft_random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shuffle_error);
    FT_ASSERT_EQ(distribution(reference), next_draw);
    while (index < CARD_COUNT)
    {
        FT_ASSERT_EQ(expected_order[index], *deck[static_cast<ft_size_t>(index)]);
        index += 1;
    }
    return (1);
}

FT_TEST(test_rng_deck_shuffle_empty_and_singleton_consume_no_draws)
{
    constexpr uint32_t SEED = 913U;
    int32_t card = 5;
    ft_deck<int32_t> deck;
    std::mt19937 reference(SEED);
    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.initialize());
    ft_seed_random_engine(SEED);
    FT_ASSERT_EQ(FT_ERR_EMPTY, deck.shuffle());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.push_back(&card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deck.shuffle());
    FT_ASSERT_EQ(card, *deck[0]);
    FT_ASSERT_EQ(distribution(reference), ft_random_int());
    return (1);
}

FT_TEST(test_rng_deck_move_transfers_heap_storage_and_thread_safety)
{
    constexpr ft_size_t CARD_COUNT = 32U;
    int32_t cards[CARD_COUNT] = {};
    int32_t previous_card = 5;
    ft_deck<int32_t> source;
    ft_deck<int32_t> destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.push_back(&previous_card));
    ft_size_t index = 0U;
    while (index < CARD_COUNT)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(&cards[index]));
        index += 1U;
    }
    int32_t **storage = source.begin();

    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source.is_initialised());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, destination.ft_vector<int32_t*>::is_thread_safe());
    FT_ASSERT_EQ(CARD_COUNT, destination.size());
    FT_ASSERT(destination.begin() == storage);
    FT_ASSERT(destination[CARD_COUNT - 1U] == &cards[CARD_COUNT - 1U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.size());
    return (1);
}

FT_TEST(test_rng_deck_rvalue_initialize_moves_inline_cards_and_handles_self_move)
{
    int32_t first_card = 1;
    int32_t second_card = 2;
    ft_deck<int32_t> source;
    ft_deck<int32_t> destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(&first_card));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(&second_card));
    int32_t **storage = source.begin();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(ft_move(source)));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source.is_initialised());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT(destination.begin() != storage);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), destination.move(destination));
    FT_ASSERT_EQ(2U, destination.size());
    FT_ASSERT(destination[0] == &first_card);
    FT_ASSERT(destination[1] == &second_card);
    return (1);
}

FT_TEST(test_rng_deck_move_from_destroyed_source)
{
    ft_deck<int32_t> source;
    ft_deck<int32_t> copy;
    ft_deck<int32_t> destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copy.is_initialised());
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), destination.move(copy));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination.is_initialised());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(0U, destination.size());
    return (1);
}
