#include "../../../Modules/Game/game_inventory.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static void populate_snapshot(game_inventory_snapshot &snapshot, int32_t item_count,
    uint64_t first_instance)
{
    snapshot.capacity = static_cast<ft_size_t>(item_count + 1);
    snapshot.used_slots = static_cast<ft_size_t>(item_count);
    snapshot.current_weight = item_count;
    snapshot.next_slot = item_count;
    snapshot.revision = 41U;
    int32_t index = 0;
    while (index < item_count)
    {
        game_inventory_item_snapshot item = {};
        item.slot = index;
        item.item_id = 42;
        item.instance_id = first_instance + static_cast<uint64_t>(index);
        item.stack_size = 1;
        item.max_stack = 1;
        item.width = 1;
        item.height = 1;
        snapshot.items.push_back(item);
        ++index;
    }
    return ;
}

static int32_t measure_transfer(int32_t item_count, uint32_t iterations)
{
    game_inventory source;
    game_inventory destination;
    game_inventory_snapshot source_snapshot = {};
    game_inventory_snapshot destination_snapshot = {};
    populate_snapshot(source_snapshot, item_count, 1U);
    populate_snapshot(destination_snapshot, item_count, 100000U);
    if (source.initialize() != FT_ERR_SUCCESS || destination.initialize() != FT_ERR_SUCCESS)
        return (1);
    std::chrono::steady_clock::duration elapsed{};
    uint32_t index = 0U;
    game_inventory_transfer_result result = {};
    while (index < iterations)
    {
        if (source.restore_snapshot(source_snapshot) != FT_ERR_SUCCESS
            || destination.restore_snapshot(destination_snapshot) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t error = source.transfer_to(destination, 0, 1,
            source_snapshot.revision, destination_snapshot.revision, 1U,
            index + 1U, &result);
        elapsed += std::chrono::steady_clock::now() - start;
        if (error != FT_ERR_SUCCESS || result.transferred_quantity != 1)
            return (1);
        ++index;
    }
    if (source.count_item(42) != item_count - 1 || destination.count_item(42) != item_count + 1)
        return (1);
    game_inventory_snapshot restored = {};
    if (destination.get_snapshot(restored) != FT_ERR_SUCCESS
        || restored.items.back().slot != item_count
        || restored.items.back().instance_id != 1U
        || restored.revision != result.destination_revision)
        return (1);
    uint64_t checksum = 0U;
    auto item = restored.items.begin();
    while (item != restored.items.end())
    {
        checksum = checksum * 31U + item->instance_id;
        ++item;
    }
    std::printf("transfer %" PRId32 " %.3f us/transfer checksum=%" PRIu64 "\n",
        item_count, std::chrono::duration<double, std::micro>(elapsed).count() / iterations,
        checksum);
    if (item_count == 8)
    {
        constexpr uint32_t REPLAYS = 100000U;
        game_inventory_transfer_result replay = {};
        index = 0U;
        const auto start = std::chrono::steady_clock::now();
        while (index < REPLAYS)
        {
            if (source.transfer_to(destination, 0, 1, source_snapshot.revision,
                    destination_snapshot.revision, 1U, result.transaction_id, &replay)
                != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        const double replay_time = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (replay.source_revision != result.source_revision
            || replay.destination_revision != result.destination_revision
            || replay.transferred_quantity != 1)
            return (1);
        std::printf("replay 8 %.3f us/transfer checksum=%" PRIu64 "\n",
            replay_time / REPLAYS, checksum);
    }
    return (0);
}

template <ft_bool Move>
static int32_t measure_lifecycle_transfer(int32_t item_count, uint32_t iterations)
{
    game_inventory_snapshot snapshot = {};
    populate_snapshot(snapshot, item_count, 1U);
    double elapsed = 0.0;
    ft_size_t allocations = 0U;
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        game_inventory source;
        game_inventory destination;
        if (source.initialize() != FT_ERR_SUCCESS
            || source.restore_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        ft_size_t before;
        ft_size_t after;
        ft_size_t frees;
        if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        int32_t result;
        if constexpr (Move == FT_TRUE)
            result = destination.move(source);
        else
            result = destination.initialize(source);
        elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        if (result != FT_ERR_SUCCESS || cma_get_stats(&after, &frees) != FT_ERR_SUCCESS
            || destination.count_item(42) != item_count)
            return (1);
        allocations += after - before;
        ++iteration;
    }
    const char *operation = "copy";
    if constexpr (Move == FT_TRUE)
        operation = "move";
    std::printf("inventory_%s %" PRId32 " %.3f %" PRIu64 "\n",
        operation, item_count, elapsed / iterations, ft_platform_cast<uint64_t>(allocations));
    return (0);
}

#ifdef LIBFT_TEST_BUILD
static int32_t measure_stack_merge(ft_bool thread_safe)
{
    game_inventory inventory;
    if (inventory.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_sharedptr<game_item> incoming(new game_item());
    if (!incoming || incoming->initialize() != FT_ERR_SUCCESS)
        return (1);
    incoming->set_item_id(42);
    incoming->set_max_stack(INT32_MAX);
    incoming->set_stack_size(1);
    incoming->set_modifier4_id(3);
    incoming->set_modifier4_value(-12);
    if (inventory.restore_serialized_item(incoming) != FT_ERR_SUCCESS)
        return (1);
    if (thread_safe == FT_TRUE)
    {
        auto &items = inventory.get_items();
        auto *entry = items.find(0);
        if (entry == items.end() || entry->value->enable_thread_safety() != FT_ERR_SUCCESS
            || incoming->enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
    }
    constexpr int32_t ITERATIONS = 100000;
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (inventory.restore_serialized_item(incoming) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const int32_t weight = inventory.get_current_weight();
    if (weight != ITERATIONS + 1 || inventory.get_items().size() != 1U)
        return (1);
    std::printf("merge_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(thread_safe),
        elapsed / ITERATIONS, weight);
    return (0);
}
#endif

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
    {
        if (measure_lifecycle_transfer<FT_TRUE>(8, 512U) != 0)
            return (1);
        return (measure_lifecycle_transfer<FT_TRUE>(1024, 16U));
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--copy") == 0)
    {
        if (measure_lifecycle_transfer<FT_FALSE>(8, 512U) != 0)
            return (1);
        return (measure_lifecycle_transfer<FT_FALSE>(1024, 16U));
    }
#ifdef LIBFT_TEST_BUILD
    if (argument_count == 2 && ft_strcmp(arguments[1], "--stack-match") == 0)
    {
        if (measure_stack_merge(FT_FALSE) != 0)
            return (1);
        return (measure_stack_merge(FT_TRUE));
    }
#endif
    if (measure_transfer(8, 256U) != 0 || measure_transfer(1024, 16U) != 0)
        return (1);
    return (0);
}
