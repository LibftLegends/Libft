#include "../../../Modules/Game/game_recipe_book.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <vector>

static int32_t measure_catalogue()
{
    constexpr uint32_t RECIPE_COUNT = 1024U;
    constexpr uint32_t ITERATIONS = 24U;
    ft_vector<uint32_t> ids;
    if (ids.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = RECIPE_COUNT;
    while (index != 0U)
    {
        if (ids.push_back(index * 31U) != FT_ERR_SUCCESS)
            return (1);
        --index;
    }
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_recipe_book book;
        uint64_t revision = 0U;
        ft_bool known = FT_TRUE;
        if (book.initialize(ids) != FT_ERR_SUCCESS
            || book.is_recipe_known(31U, &known) != FT_ERR_SUCCESS || known != FT_FALSE
            || book.get_revision(&revision) != FT_ERR_SUCCESS)
            return (1);
        checksum += revision;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("catalogue %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_replay(game_recipe_book &book, uint32_t event_count)
{
    constexpr uint32_t ITERATIONS = 65536U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        game_recipe_unlock_result result = {};
        if (book.unlock_recipe_with_result(2U, event_count, &result) != FT_ERR_SUCCESS
            || result.event_newly_recorded != FT_FALSE || result.recipe_newly_known != FT_FALSE)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t revision = 0U;
    if (book.get_revision(&revision) != FT_ERR_SUCCESS)
        return (1);
    std::printf("replay %.3f %" PRIu64 "\n", elapsed / ITERATIONS, revision);
    return (0);
}

static int32_t measure_reads(const game_recipe_book &book)
{
    constexpr uint32_t ITERATIONS = 4096U;
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        uint64_t revision = 0U;
        uint32_t quarantined = 0U;
        ft_bool known = FT_FALSE;
        if (book.get_revision(&revision) != FT_ERR_SUCCESS
            || book.is_recipe_known(2U, &known) != FT_ERR_SUCCESS
            || book.quarantined_unlock_count(&quarantined) != FT_ERR_SUCCESS
            || known != FT_TRUE || quarantined != 0U)
            return (1);
        checksum += revision;
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("reads %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_import(game_recipe_book &book, ft_byte_buffer &payload,
    uint64_t expected_revision)
{
    constexpr uint32_t ITERATIONS = 12U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (payload.set_read_position(0U) != FT_ERR_SUCCESS || book.import_book(payload) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint32_t quarantined = 0U;
    uint64_t revision = 0U;
    if (book.get_revision(&revision) != FT_ERR_SUCCESS || revision != expected_revision
        || book.quarantined_unlock_count(&quarantined) != FT_ERR_SUCCESS || quarantined != 1U)
        return (1);
    std::printf("import %.3f %" PRIu64 "\n", elapsed / ITERATIONS, revision);
    return (0);
}

static int32_t measure_export(const game_recipe_book &book, ft_byte_buffer &payload)
{
    constexpr uint32_t ITERATIONS = 512U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (payload.clear() != FT_ERR_SUCCESS || book.export_book(payload) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const uint8_t *bytes = payload.data();
    const ft_size_t length = payload.size();
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < length)
        checksum = checksum * 31U + bytes[index++];
    std::printf("export %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    constexpr uint32_t EVENT_COUNT = 512U;
    ft_vector<uint32_t> ids;
    ft_vector<uint32_t> target_ids;
    game_recipe_book source;
    game_recipe_book target;
    ft_byte_buffer payload;
    std::vector<game_recipe_unlock_record> events(EVENT_COUNT);
    uint32_t index = 0U;
    while (index < EVENT_COUNT)
    {
        events[index] = {static_cast<uint64_t>(index) + 1U, 1U};
        ++index;
    }
    events.back().recipe_id = 2U;
    uint64_t revision = 0U;
    if (ids.initialize() != FT_ERR_SUCCESS || ids.push_back(1U) != FT_ERR_SUCCESS
        || ids.push_back(2U) != FT_ERR_SUCCESS || target_ids.initialize() != FT_ERR_SUCCESS
        || target_ids.push_back(1U) != FT_ERR_SUCCESS || target.initialize(target_ids) != FT_ERR_SUCCESS
        || source.initialize(ids, events.data(), EVENT_COUNT) != FT_ERR_SUCCESS
        || payload.initialize() != FT_ERR_SUCCESS
        || source.get_revision(&revision) != FT_ERR_SUCCESS || measure_reads(source) != 0
        || measure_catalogue() != 0 || measure_replay(source, EVENT_COUNT) != 0
        || measure_export(source, payload) != 0)
        return (1);
    return (measure_import(target, payload, revision));
}
