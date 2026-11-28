#include "../../../Modules/Game/game_economy_table.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_catalog(ft_bool map_locked)
{
    constexpr int32_t PRICE_COUNT = 512;
    constexpr uint32_t QUERIES = 100000U;
    constexpr uint32_t REPLACEMENTS = 400U;
    game_economy_table source;
    game_economy_table target;
    game_price_definition price;
    if (source.initialize() != FT_ERR_SUCCESS || target.initialize() != FT_ERR_SUCCESS
        || price.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (map_locked && target.get_price_definitions().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    int32_t item_id = 0;
    while (item_id < PRICE_COUNT)
    {
        price.set_item_id(item_id);
        price.set_base_value(item_id * 9 + 20);
        if (source.register_price_definition(price) != FT_ERR_SUCCESS)
            return (1);
        ++item_id;
    }
    target.set_price_definitions(source.get_price_definitions());
    if (target.get_error() != FT_ERR_SUCCESS)
        return (1);
    uint32_t count = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint64_t checksum = 0U;
    while (count < QUERIES)
    {
        if (target.fetch_price_definition(static_cast<int32_t>(count % PRICE_COUNT), price)
            != FT_ERR_SUCCESS)
            return (1);
        checksum += price.get_base_value();
        ++count;
    }
    const auto replace_start = std::chrono::steady_clock::now();
    count = 0U;
    while (count < REPLACEMENTS)
    {
        target.set_price_definitions(source.get_price_definitions());
        if (target.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++count;
    }
    const auto end = std::chrono::steady_clock::now();
    if (target.get_price_definitions().size() != static_cast<ft_size_t>(PRICE_COUNT))
        return (1);
    std::printf("map_lock=%u query_ns=%.3f replace_us=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(map_locked),
        std::chrono::duration<double, std::nano>(replace_start - start).count() / QUERIES,
        std::chrono::duration<double, std::micro>(end - replace_start).count() / REPLACEMENTS,
        checksum);
    if (map_locked && (source.enable_thread_safety() != FT_ERR_SUCCESS
        || source.get_price_definitions().enable_thread_safety() != FT_ERR_SUCCESS))
        return (1);
    const auto copy_start = std::chrono::steady_clock::now();
    count = 0U;
    uint64_t copy_checksum = 0U;
    while (count < REPLACEMENTS)
    {
        if (target.initialize(source) != FT_ERR_SUCCESS
            || target.get_price_definitions().size() != static_cast<ft_size_t>(PRICE_COUNT)
            || target.fetch_price_definition(static_cast<int32_t>(count % PRICE_COUNT),
                price) != FT_ERR_SUCCESS)
            return (1);
        copy_checksum += price.get_base_value();
        ++count;
    }
    const auto copy_end = std::chrono::steady_clock::now();
    std::printf("source_lock=%u copy_us=%.3f copy_checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(map_locked),
        std::chrono::duration<double, std::micro>(copy_end - copy_start).count() / REPLACEMENTS,
        copy_checksum);
    game_economy_table *move_source = &target;
    game_economy_table *move_destination = &source;
    std::chrono::steady_clock::duration move_duration{};
    count = 0U;
    uint64_t move_checksum = 0U;
    while (count < REPLACEMENTS)
    {
        if (map_locked && (move_source->enable_thread_safety() != FT_ERR_SUCCESS
            || move_source->get_price_definitions().enable_thread_safety() != FT_ERR_SUCCESS))
            return (1);
        const auto move_start = std::chrono::steady_clock::now();
        const int32_t result = move_destination->move(*move_source);
        move_duration += std::chrono::steady_clock::now() - move_start;
        if (result != FT_ERR_SUCCESS
            || move_destination->get_price_definitions().size() != static_cast<ft_size_t>(PRICE_COUNT)
            || move_destination->fetch_price_definition(static_cast<int32_t>(count % PRICE_COUNT),
                price) != FT_ERR_SUCCESS)
            return (1);
        move_checksum += price.get_base_value();
        game_economy_table *previous_source = move_source;
        move_source = move_destination;
        move_destination = previous_source;
        ++count;
    }
    std::printf("source_lock=%u move_us=%.3f move_checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(map_locked),
        std::chrono::duration<double, std::micro>(move_duration).count() / REPLACEMENTS,
        move_checksum);
    return (0);
}

int32_t main()
{
    if (measure_catalog(FT_FALSE) != 0)
        return (1);
    return (measure_catalog(FT_TRUE));
}
