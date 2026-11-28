#include "../../../Modules/Game/game_reputation.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

using reputation_clock = std::chrono::steady_clock;

static void report(const char *operation, ft_bool locked, reputation_clock::duration duration,
    uint32_t iterations, uint64_t checksum)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        duration).count();
    std::printf("%s_%u %.3f %" PRIu64 "\n", operation,
        static_cast<uint32_t>(locked), elapsed / iterations, checksum);
    return ;
}

static int32_t measure(ft_bool locked)
{
    constexpr uint32_t ITERATIONS = 100000U;
    constexpr int32_t LAST_ID = 128;
    game_reputation reputation;
    if (reputation.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (locked == FT_TRUE && (reputation.get_reps().enable_thread_safety() != FT_ERR_SUCCESS
        || reputation.get_milestones().enable_thread_safety() != FT_ERR_SUCCESS))
        return (1);
    int32_t id = 1;
    while (id <= LAST_ID)
    {
        reputation.set_rep(id, id + 1);
        reputation.set_milestone(id, id + 2);
        ++id;
    }
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    auto start = reputation_clock::now();
    while (iteration < ITERATIONS)
    {
        checksum += reputation.get_rep(LAST_ID);
        checksum += reputation.get_milestone(LAST_ID);
        ++iteration;
    }
    report("read_pair", locked, reputation_clock::now() - start, ITERATIONS, checksum);
    if (checksum != ITERATIONS * (2U * LAST_ID + 3U))
        return (1);

    iteration = 0U;
    start = reputation_clock::now();
    while (iteration < ITERATIONS)
    {
        reputation.set_rep(LAST_ID, static_cast<int32_t>(iteration));
        reputation.set_milestone(LAST_ID, static_cast<int32_t>(iteration + 7U));
        ++iteration;
    }
    const auto elapsed = reputation_clock::now() - start;
    checksum = static_cast<uint64_t>(reputation.get_rep(LAST_ID))
        + reputation.get_milestone(LAST_ID);
    report("update_pair", locked, elapsed, ITERATIONS, checksum);
    if (checksum != 2U * ITERATIONS + 5U)
        return (1);
    return (0);
}

static int32_t measure_inserts(ft_bool locked)
{
    constexpr uint32_t ROUNDS = 500U;
    constexpr int32_t LAST_ID = 128;
    reputation_clock::duration elapsed{};
    uint64_t checksum = 0U;
    uint32_t round = 0U;
    while (round < ROUNDS)
    {
        game_reputation reputation;
        if (reputation.initialize() != FT_ERR_SUCCESS)
            return (1);
        if (locked == FT_TRUE && (reputation.get_reps().enable_thread_safety() != FT_ERR_SUCCESS
            || reputation.get_milestones().enable_thread_safety() != FT_ERR_SUCCESS))
            return (1);
        int32_t id = 1;
        const auto start = reputation_clock::now();
        while (id <= LAST_ID)
        {
            reputation.set_rep(id, id + 1);
            reputation.set_milestone(id, id + 2);
            ++id;
        }
        elapsed += reputation_clock::now() - start;
        checksum += reputation.get_rep(LAST_ID);
        checksum += reputation.get_milestone(LAST_ID);
        ++round;
    }
    report("insert_pair", locked, elapsed, ROUNDS * LAST_ID, checksum);
    if (checksum != ROUNDS * (2U * LAST_ID + 3U))
        return (1);
    return (0);
}

static int32_t measure_moves()
{
    constexpr uint32_t ROUNDS = 512U;
    constexpr int32_t LAST_ID = 1024;
    game_reputation fixture;
    game_reputation destination;
    if (fixture.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t id = 1;
    while (id <= LAST_ID)
    {
        fixture.set_rep(id, id + 1);
        fixture.set_milestone(id, id + 2);
        ++id;
    }
    reputation_clock::duration elapsed{};
    uint64_t checksum = 0U;
    uint32_t round = 0U;
    while (round < ROUNDS)
    {
        game_reputation source;
        if (source.initialize(fixture) != FT_ERR_SUCCESS)
            return (1);
        const auto start = reputation_clock::now();
        const int32_t result = destination.move(source);
        elapsed += reputation_clock::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum += destination.get_rep(LAST_ID);
        checksum += destination.get_milestone(LAST_ID);
        ++round;
    }
    report("move", FT_FALSE, elapsed, ROUNDS, checksum);
    if (checksum != ROUNDS * (2U * LAST_ID + 3U))
        return (1);
    return (0);
}

static int32_t measure_counters(ft_bool locked)
{
    constexpr uint32_t ITERATIONS = 200000U;
    game_reputation reputation;
    if (reputation.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (locked == FT_TRUE && reputation.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    reputation.set_total_rep(24);
    reputation.set_current_rep(18);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    auto start = reputation_clock::now();
    while (iteration < ITERATIONS)
    {
        checksum += reputation.get_total_rep();
        checksum += reputation.get_current_rep();
        ++iteration;
    }
    report("counter_reads", locked, reputation_clock::now() - start, ITERATIONS, checksum);
    if (checksum != ITERATIONS * 42U)
        return (1);
    iteration = 0U;
    start = reputation_clock::now();
    while (iteration < ITERATIONS)
    {
        reputation.add_total_rep(3);
        reputation.sub_total_rep(2);
        reputation.add_current_rep(-2);
        reputation.sub_current_rep(-3);
        ++iteration;
    }
    const auto elapsed = reputation_clock::now() - start;
    checksum = static_cast<uint64_t>(reputation.get_total_rep())
        + reputation.get_current_rep();
    report("counter_adjustments", locked, elapsed, ITERATIONS, checksum);
    if (checksum != 2U * ITERATIONS + 42U)
        return (1);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
        return (measure_moves());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--counters") == 0)
    {
        if (measure_counters(FT_FALSE) != 0)
            return (1);
        return (measure_counters(FT_TRUE));
    }
    if (measure(FT_FALSE) != 0 || measure(FT_TRUE) != 0
        || measure_inserts(FT_FALSE) != 0)
        return (1);
    return (measure_inserts(FT_TRUE));
}
