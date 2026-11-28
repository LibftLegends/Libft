#include "../test_internal.hpp"
#include "../../Modules/Game/game_behavior_tree.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Template/function.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <memory>
#include <new>

static void make_behavior_action(ft_sharedptr<game_behavior_node> &action,
    int32_t status)
{
    game_behavior_tree_action *action_pointer;

    action_pointer = new game_behavior_tree_action();
    action.initialize(action_pointer);
    action_pointer->set_callback(ft_function<int32_t(game_behavior_context &)>(
        [status](game_behavior_context &) -> int32_t
        {
            return (status);
        }));
    return ;
}

FT_TEST(test_game_behavior_context_defaults)
{
    game_behavior_context context;
    FT_ASSERT_EQ(ft_nullptr, context.get_character());
    FT_ASSERT_EQ(ft_nullptr, context.get_user_data());
    return (1);
}

FT_TEST(test_game_behavior_context_user_data)
{
    game_behavior_context context;
    void *user_data = reinterpret_cast<void *>(static_cast<uintptr_t>(2U));
    context.set_user_data(user_data);
    FT_ASSERT_EQ(user_data, context.get_user_data());
    return (1);
}

FT_TEST(test_game_behavior_context_copy)
{
    game_behavior_context source;
    void *user_data = reinterpret_cast<void *>(static_cast<uintptr_t>(3U));
    source.set_user_data(user_data);
    game_behavior_context copy(source);
    FT_ASSERT_EQ(user_data, copy.get_user_data());
    return (1);
}

FT_TEST(test_game_behavior_context_move_clears_source)
{
    game_behavior_context source;
    void *user_data = reinterpret_cast<void *>(static_cast<uintptr_t>(4U));
    source.set_user_data(user_data);
    game_behavior_context moved(static_cast<game_behavior_context &&>(source));
    FT_ASSERT_EQ(user_data, moved.get_user_data());
    FT_ASSERT_EQ(ft_nullptr, source.get_user_data());
    return (1);
}

FT_TEST(test_game_behavior_action_without_callback)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, action.tick(context));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, action.get_error());
    return (1);
}

FT_TEST(test_game_behavior_action_running)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) -> int32_t { return (FT_BEHAVIOR_STATUS_RUNNING); }));
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_RUNNING, action.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_action_success)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) -> int32_t { return (FT_BEHAVIOR_STATUS_SUCCESS); }));
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, action.tick(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, action.get_error());
    return (1);
}

FT_TEST(test_game_behavior_action_failure)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) -> int32_t { return (FT_BEHAVIOR_STATUS_FAILURE); }));
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, action.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_action_invalid_status)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) -> int32_t { return (99); }));
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, action.tick(context));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, action.get_error());
    return (1);
}

FT_TEST(test_game_behavior_action_callback_getter)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) -> int32_t { return (FT_BEHAVIOR_STATUS_SUCCESS); }));
    FT_ASSERT(static_cast<ft_bool>(action.get_callback()));
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, action.get_callback()(context));
    return (1);
}

FT_TEST(test_game_behavior_selector_empty)
{
    game_behavior_selector selector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, selector.initialize());
    game_behavior_context context;
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, selector.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_sequence_empty)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    game_behavior_context context;
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, sequence.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_composite_null_child)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    ft_sharedptr<game_behavior_node> child;
    sequence.add_child(child);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sequence.get_error());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), sequence.get_children().size());
    return (1);
}

FT_TEST(test_game_behavior_composite_add_and_clear)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    ft_sharedptr<game_behavior_node> child;
    make_behavior_action(child, FT_BEHAVIOR_STATUS_SUCCESS);
    sequence.add_child(child);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), sequence.get_children().size());
    sequence.clear_children();
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), sequence.get_children().size());
    return (1);
}

FT_TEST(test_game_behavior_selector_success)
{
    game_behavior_selector selector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, selector.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> child;
    make_behavior_action(child, FT_BEHAVIOR_STATUS_SUCCESS);
    selector.add_child(child);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, selector.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_selector_failure_then_success)
{
    game_behavior_selector selector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, selector.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_FAILURE);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_SUCCESS);
    selector.add_child(first);
    selector.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, selector.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_selector_running_stops)
{
    game_behavior_selector selector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, selector.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_RUNNING);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_SUCCESS);
    selector.add_child(first);
    selector.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_RUNNING, selector.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_selector_all_failure)
{
    game_behavior_selector selector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, selector.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_FAILURE);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_FAILURE);
    selector.add_child(first);
    selector.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, selector.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_sequence_success)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> child;
    make_behavior_action(child, FT_BEHAVIOR_STATUS_SUCCESS);
    sequence.add_child(child);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, sequence.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_sequence_failure_stops)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_FAILURE);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_SUCCESS);
    sequence.add_child(first);
    sequence.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, sequence.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_sequence_running_stops)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_RUNNING);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_FAILURE);
    sequence.add_child(first);
    sequence.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_RUNNING, sequence.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_sequence_all_success)
{
    game_behavior_sequence sequence;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> first;
    ft_sharedptr<game_behavior_node> second;
    make_behavior_action(first, FT_BEHAVIOR_STATUS_SUCCESS);
    make_behavior_action(second, FT_BEHAVIOR_STATUS_SUCCESS);
    sequence.add_child(first);
    sequence.add_child(second);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, sequence.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_tree_without_root)
{
    game_behavior_tree tree;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    game_behavior_context context;
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, tree.tick(context));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, tree.get_error());
    return (1);
}

FT_TEST(test_game_behavior_tree_root_round_trip)
{
    game_behavior_tree tree;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    ft_sharedptr<game_behavior_node> root;
    make_behavior_action(root, FT_BEHAVIOR_STATUS_SUCCESS);
    tree.set_root(root);
    FT_ASSERT(tree.get_root());
    return (1);
}

FT_TEST(test_game_behavior_tree_success)
{
    game_behavior_tree tree;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> root;
    make_behavior_action(root, FT_BEHAVIOR_STATUS_SUCCESS);
    tree.set_root(root);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, tree.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_tree_running)
{
    game_behavior_tree tree;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> root;
    make_behavior_action(root, FT_BEHAVIOR_STATUS_RUNNING);
    tree.set_root(root);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_RUNNING, tree.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_tree_failure)
{
    game_behavior_tree tree;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> root;
    make_behavior_action(root, FT_BEHAVIOR_STATUS_FAILURE);
    tree.set_root(root);
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, tree.tick(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.get_error());
    return (1);
}

FT_TEST(test_game_behavior_tree_lifecycle)
{
    game_behavior_tree tree;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, tree.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.destroy());
    return (1);
}

FT_TEST(test_game_behavior_composite_requires_initialization)
{
    game_behavior_sequence sequence;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.destroy());
    sequence.clear_children();
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, sequence.get_error());
    return (1);
}

FT_TEST(test_game_behavior_sequence_propagates_child_storage_lock_failure)
{
    game_behavior_sequence sequence;
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> child;
    make_behavior_action(child, FT_BEHAVIOR_STATUS_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    sequence.add_child(child);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.get_children().enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t tick_status = sequence.tick(context);
    const int32_t tick_error = sequence.get_error();
    sequence.add_child(child);
    const int32_t add_error = sequence.get_error();
    sequence.clear_children();
    const int32_t clear_error = sequence.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, tick_status);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, tick_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, add_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, clear_error);
    FT_ASSERT_EQ(1U, sequence.get_children().size());
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, sequence.tick(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.get_error());
    return (1);
}

FT_TEST(test_game_behavior_callback_can_replace_itself_during_a_tick)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    auto lifetime = std::make_shared<uint32_t>(0U);
    std::weak_ptr<uint32_t> observer(lifetime);
    ft_bool retained = FT_FALSE;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [lifetime, &action, &observer, &retained](game_behavior_context &)
        {
            action.set_callback(ft_function<int32_t(game_behavior_context &)>(
                [](game_behavior_context &) { return (FT_BEHAVIOR_STATUS_RUNNING); }));
            retained = !observer.expired();
            return (FT_BEHAVIOR_STATUS_SUCCESS);
        }));
    lifetime.reset();

    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, action.tick(context));
    FT_ASSERT_EQ(FT_TRUE, retained);
    FT_ASSERT(observer.expired());
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_RUNNING, action.tick(context));
    return (1);
}

struct behavior_callback_copy_failure
{
    const ft_bool &fail_copy;

    explicit behavior_callback_copy_failure(const ft_bool &flag) noexcept
        : fail_copy(flag)
    {
        return ;
    }

    behavior_callback_copy_failure(const behavior_callback_copy_failure &other)
        : fail_copy(other.fail_copy)
    {
        if (this->fail_copy)
            throw std::bad_alloc();
        return ;
    }

    int32_t operator()(game_behavior_context &) const noexcept
    {
        return (FT_BEHAVIOR_STATUS_RUNNING);
    }
};

FT_TEST(test_game_behavior_callback_copy_failure_preserves_previous_callback)
{
    game_behavior_tree_action action;
    game_behavior_context context;
    action.set_callback(ft_function<int32_t(game_behavior_context &)>(
        [](game_behavior_context &) { return (FT_BEHAVIOR_STATUS_SUCCESS); }));
    ft_bool fail_copy = FT_FALSE;
    const ft_function<int32_t(game_behavior_context &)> replacement{
        behavior_callback_copy_failure(fail_copy)};
    fail_copy = FT_TRUE;
    action.set_callback(replacement);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, action.get_error());
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, action.tick(context));
    return (1);
}

FT_TEST(test_game_behavior_tree_retains_active_root_during_replacement)
{
    game_behavior_tree tree;
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> root;
    ft_sharedptr<game_behavior_node> empty;
    auto lifetime = std::make_shared<uint32_t>(0U);
    std::weak_ptr<uint32_t> observer(lifetime);
    ft_bool retained = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    auto *leaf = new game_behavior_tree_action();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, root.initialize(leaf));
    leaf->set_callback(ft_function<int32_t(game_behavior_context &)>(
        [lifetime, &tree, &empty, &observer, &retained](game_behavior_context &)
        {
            tree.set_root(empty);
            retained = !observer.expired();
            return (FT_BEHAVIOR_STATUS_SUCCESS);
        }));
    lifetime.reset();
    tree.set_root(root);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, root.destroy());

    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, tree.tick(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tree.get_error());
    FT_ASSERT_EQ(FT_TRUE, retained);
    FT_ASSERT(observer.expired());
    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_FAILURE, tree.tick(context));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, tree.get_error());
    return (1);
}

FT_TEST(test_game_behavior_sequence_retains_child_during_clear)
{
    game_behavior_sequence sequence;
    game_behavior_context context;
    ft_sharedptr<game_behavior_node> child;
    auto lifetime = std::make_shared<uint32_t>(0U);
    std::weak_ptr<uint32_t> observer(lifetime);
    ft_bool retained = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.initialize());
    auto *leaf = new game_behavior_tree_action();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child.initialize(leaf));
    leaf->set_callback(ft_function<int32_t(game_behavior_context &)>(
        [lifetime, &sequence, &observer, &retained](game_behavior_context &)
        {
            sequence.clear_children();
            retained = !observer.expired();
            return (FT_BEHAVIOR_STATUS_SUCCESS);
        }));
    lifetime.reset();
    sequence.add_child(child);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child.destroy());

    FT_ASSERT_EQ(FT_BEHAVIOR_STATUS_SUCCESS, sequence.tick(context));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sequence.get_error());
    FT_ASSERT_EQ(FT_TRUE, retained);
    FT_ASSERT(observer.expired());
    FT_ASSERT_EQ(0U, sequence.get_children().size());
    return (1);
}
