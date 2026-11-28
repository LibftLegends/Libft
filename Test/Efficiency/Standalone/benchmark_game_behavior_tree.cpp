#include "../../../Modules/Game/game_behavior_tree.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static int32_t measure_tree(game_behavior_composite *branch, const char *name,
    int32_t continuing_status, int32_t final_status)
{
    constexpr uint32_t CHILD_COUNT = 32U;
    constexpr uint32_t TICKS = 50000U;
    game_behavior_tree tree;
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> root;
    ft_sharedptr<game_behavior_node> child;
    if (branch == ft_nullptr || root.initialize(branch) != FT_ERR_SUCCESS)
        return (1);
    if (branch->initialize() != FT_ERR_SUCCESS || tree.initialize() != FT_ERR_SUCCESS)
        return (1);
    auto *leaf = new (std::nothrow) game_behavior_tree_action();
    if (leaf == ft_nullptr || child.initialize(leaf) != FT_ERR_SUCCESS)
        return (1);
    uint64_t visits = 0U;
    leaf->set_callback(ft_function<int32_t(game_behavior_context &)>(
        [&visits, continuing_status](game_behavior_context &) noexcept
        {
            ++visits;
            return (continuing_status);
        }));
    uint32_t index = 0U;
    while (index < CHILD_COUNT)
    {
        branch->add_child(child);
        if (branch->get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    tree.set_root(root);
    if (tree.get_error() != FT_ERR_SUCCESS)
        return (1);

    uint64_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < TICKS)
    {
        const int32_t status = tree.tick(context);
        if (status != final_status || tree.get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum += status;
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    if (visits != static_cast<uint64_t>(CHILD_COUNT) * TICKS)
        return (1);
    std::printf("%s tick_ns=%.3f visits=%" PRIu64 " checksum=%" PRIu64 "\n", name,
        std::chrono::duration<double, std::nano>(end - start).count() / TICKS,
        visits, checksum);
    return (0);
}

int32_t main()
{
    if (measure_tree(new (std::nothrow) game_behavior_selector(), "selector",
        FT_BEHAVIOR_STATUS_FAILURE, FT_BEHAVIOR_STATUS_FAILURE) != 0)
        return (1);
    return (measure_tree(new (std::nothrow) game_behavior_sequence(), "sequence",
        FT_BEHAVIOR_STATUS_SUCCESS, FT_BEHAVIOR_STATUS_SUCCESS));
}
