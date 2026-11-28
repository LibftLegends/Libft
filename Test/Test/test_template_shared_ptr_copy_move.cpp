#include "../test_internal.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

struct s_shared_lifetime_probe
{
    static uint32_t constructions;
    std::atomic<uint32_t> *destructions;

    s_shared_lifetime_probe();
    ~s_shared_lifetime_probe();
};

uint32_t s_shared_lifetime_probe::constructions = 0U;

s_shared_lifetime_probe::s_shared_lifetime_probe() : destructions(ft_nullptr)
{
    ++constructions;
    return ;
}

s_shared_lifetime_probe::~s_shared_lifetime_probe()
{
    if (this->destructions != ft_nullptr)
        this->destructions->fetch_add(1U);
    return ;
}

FT_TEST(test_ft_sharedptr_retained_visit_releases_array_after_handle_reset)
{
    std::atomic<uint32_t> destructions(0U);
    auto *array = new s_shared_lifetime_probe[2];
    array[0].destructions = &destructions;
    array[1].destructions = &destructions;
    ft_sharedptr<s_shared_lifetime_probe> handle(array, FT_TRUE, 2U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.enable_thread_safety());

    ft_bool visited = FT_FALSE;
    uint32_t destroyed_during_visit = 2U;
    const int32_t result = handle.visit_retained(
        [&handle, &destructions, &visited, &destroyed_during_visit, array](
            s_shared_lifetime_probe &value) noexcept
        {
            visited = &value == array;
            handle.reset();
            destroyed_during_visit = destructions.load();
            return ;
        });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_TRUE, visited);
    FT_ASSERT_EQ(0U, destroyed_during_visit);
    FT_ASSERT_EQ(2U, destructions.load());
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, handle.visit_retained(
        [&visited](s_shared_lifetime_probe &) noexcept
        {
            visited = FT_FALSE;
            return ;
        }));
    FT_ASSERT_EQ(FT_TRUE, visited);
    return (1);
}

FT_TEST(test_ft_sharedptr_retained_visit_propagates_handle_lock_failure)
{
    ft_sharedptr<uint32_t> handle(new uint32_t(17U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.enable_thread_safety());
    ft_bool visited = FT_FALSE;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = handle.visit_retained(
        [&visited](uint32_t &) noexcept
        {
            visited = FT_TRUE;
            return ;
        });
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_FALSE, visited);
    FT_ASSERT_EQ(1, handle.use_count());
    return (1);
}

FT_TEST(test_ft_sharedptr_const_retained_visit_keeps_object_alive)
{
    std::atomic<uint32_t> destructions(0U);
    auto *probe = new s_shared_lifetime_probe();
    probe->destructions = &destructions;
    ft_sharedptr<s_shared_lifetime_probe> owner(probe);
    const auto &read_only_owner = owner;
    uint32_t destroyed_during_visit = 1U;
    const int32_t result = read_only_owner.visit_retained(
        [&owner, &destroyed_during_visit](const s_shared_lifetime_probe &value) noexcept
        {
            owner.reset();
            destroyed_during_visit = value.destructions->load();
        });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(0U, destroyed_during_visit);
    FT_ASSERT_EQ(1U, destructions.load());
    return (1);
}

FT_TEST(test_ft_sharedptr_proxies_and_concurrent_release_preserve_lifetime)
{
    std::atomic<uint32_t> destructions(0U);
    ft_sharedptr<s_shared_lifetime_probe> first(new s_shared_lifetime_probe());
    ft_sharedptr<s_shared_lifetime_probe> second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(first));
    const uint32_t constructions = s_shared_lifetime_probe::constructions;
    s_shared_lifetime_probe &value = *first;
    value.destructions = &destructions;
    const ft_sharedptr<s_shared_lifetime_probe> &const_second = second;
    const s_shared_lifetime_probe &const_value = *const_second;
    FT_ASSERT_EQ(&value, &const_value);
    FT_ASSERT_EQ(constructions, s_shared_lifetime_probe::constructions);
    std::thread first_release([&first]()
    {
        first.reset();
        return ;
    });
    std::thread second_release([&second]()
    {
        second.reset();
        return ;
    });
    first_release.join();
    second_release.join();
    FT_ASSERT_EQ(1U, destructions.load());
    return (1);
}

FT_TEST(test_ft_sharedptr_alias_assignment_preserves_shared_array_and_wrapper_mutex)
{
    ft_sharedptr<uint32_t> source(new uint32_t[2]{17U, 29U}, FT_TRUE, 2U);
    ft_sharedptr<uint32_t> destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    destination = source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT(destination.is_thread_safe());
    FT_ASSERT_EQ(2, destination.use_count());
    source.reset();
    FT_ASSERT_EQ(1, destination.use_count());
    FT_ASSERT_EQ(29U, static_cast<uint32_t &>(destination[1U]));
    return (1);
}

FT_TEST(test_ft_sharedptr_copy_constructor_rebuilds_mutex)
{
    ft_sharedptr<int> source_pointer(new int(31));
    ft_sharedptr<int> copied_pointer;
    ft_bool source_lock_acquired;
    ft_bool copy_lock_acquired;

    source_lock_acquired = FT_FALSE;
    copy_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_pointer.initialize(source_pointer));
    FT_ASSERT_EQ(0, source_pointer.enable_thread_safety());
    FT_ASSERT_EQ(0, copied_pointer.enable_thread_safety());
    FT_ASSERT(source_pointer.is_thread_safe());
    FT_ASSERT(copied_pointer.is_thread_safe());
    FT_ASSERT_EQ(2, copied_pointer.use_count());
    FT_ASSERT_EQ(2, source_pointer.use_count());
    FT_ASSERT_EQ(0, source_pointer.lock(&source_lock_acquired));
    FT_ASSERT(source_lock_acquired);
    FT_ASSERT_EQ(0, copied_pointer.lock(&copy_lock_acquired));
    FT_ASSERT(copy_lock_acquired);
    FT_ASSERT_EQ(31, *source_pointer);
    FT_ASSERT_EQ(31, *copied_pointer);
    copied_pointer.unlock(copy_lock_acquired);
    source_pointer.unlock(source_lock_acquired);
    return (1);
}

FT_TEST(test_ft_sharedptr_copy_assignment_recreates_mutex)
{
    ft_sharedptr<int> source_pointer(new int(7));
    ft_sharedptr<int> destination_pointer(new int(3));
    ft_bool source_lock_acquired;
    ft_bool destination_lock_acquired;

    source_lock_acquired = FT_FALSE;
    destination_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, source_pointer.enable_thread_safety());
    FT_ASSERT_EQ(0, destination_pointer.enable_thread_safety());
    FT_ASSERT(source_pointer.is_thread_safe());
    FT_ASSERT(destination_pointer.is_thread_safe());
    destination_pointer = source_pointer;
    FT_ASSERT(destination_pointer.is_thread_safe());
    FT_ASSERT(source_pointer.is_thread_safe());
    FT_ASSERT_EQ(2, destination_pointer.use_count());
    FT_ASSERT_EQ(2, source_pointer.use_count());
    FT_ASSERT_EQ(0, source_pointer.lock(&source_lock_acquired));
    FT_ASSERT(source_lock_acquired);
    FT_ASSERT_EQ(0, destination_pointer.lock(&destination_lock_acquired));
    FT_ASSERT(destination_lock_acquired);
    FT_ASSERT_EQ(7, *destination_pointer);
    FT_ASSERT_EQ(7, *source_pointer);
    destination_pointer.unlock(destination_lock_acquired);
    source_pointer.unlock(source_lock_acquired);
    return (1);
}

FT_TEST(test_ft_sharedptr_move_constructor_rebuilds_mutex)
{
    ft_sharedptr<int> source_pointer(new int(88));
    ft_bool moved_lock_acquired;

    moved_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, source_pointer.enable_thread_safety());
    FT_ASSERT(source_pointer.is_thread_safe());
    ft_sharedptr<int> moved_pointer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_pointer.move(source_pointer));
    FT_ASSERT(moved_pointer.is_thread_safe());
    FT_ASSERT_EQ(0, moved_pointer.lock(&moved_lock_acquired));
    FT_ASSERT(moved_lock_acquired);
    FT_ASSERT_EQ(88, *moved_pointer);
    moved_pointer.unlock(moved_lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_pointer.initialize());
    FT_ASSERT_EQ(false, source_pointer.is_thread_safe());
    return (1);
}

FT_TEST(test_ft_sharedptr_move_assignment_rebuilds_mutex)
{
    ft_sharedptr<int> destination_pointer(new int(12));
    ft_sharedptr<int> source_pointer(new int(23));
    ft_bool destination_lock_acquired;

    destination_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, destination_pointer.enable_thread_safety());
    FT_ASSERT_EQ(0, source_pointer.enable_thread_safety());
    FT_ASSERT(destination_pointer.is_thread_safe());
    FT_ASSERT(source_pointer.is_thread_safe());
    destination_pointer = ft_move(source_pointer);
    FT_ASSERT(destination_pointer.is_thread_safe());
    FT_ASSERT_EQ(0, destination_pointer.lock(&destination_lock_acquired));
    FT_ASSERT(destination_lock_acquired);
    FT_ASSERT_EQ(23, *destination_pointer);
    destination_pointer.unlock(destination_lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_pointer.initialize());
    FT_ASSERT_EQ(false, source_pointer.is_thread_safe());
    return (1);
}
