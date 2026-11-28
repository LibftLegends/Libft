#include "../test_internal.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int map_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_map_get_error_returned = FT_FALSE;
static int32_t g_map_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_map_get_error_str_returned = FT_FALSE;
static const char *g_map_get_error_str_result = ft_nullptr;

static void map_get_error_uninitialised_operation(void)
{
    ft_map<int, int> map_value;

    g_map_get_error_result = map_value.get_error();
    g_map_get_error_returned = FT_TRUE;
    return ;
}

static void map_get_error_str_uninitialised_operation(void)
{
    ft_map<int, int> map_value;

    g_map_get_error_str_result = map_value.get_error_str();
    g_map_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_map_thread_safety_toggles_mutex)
{
    ft_map<int, int> map_instance;
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize());
    FT_ASSERT_EQ(false, map_instance.is_thread_safe());
    lock_acquired = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.lock(&lock_acquired));
    FT_ASSERT_EQ(FT_FALSE, lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.unlock(lock_acquired));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(true, map_instance.is_thread_safe());
    map_instance.insert(1, 2);
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.lock(&lock_acquired));
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.unlock(lock_acquired));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.disable_thread_safety());
    FT_ASSERT_EQ(false, map_instance.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(true, map_instance.is_thread_safe());
    return (1);
}

FT_TEST(test_map_move_transfers_thread_safety)
{
    ft_map<int, int> original;
    ft_map<int, int> moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize());
    original.insert(3, 9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move_from(original));
    Pair<int, int> *found_entry;

    found_entry = moved.find(3);
    FT_ASSERT(found_entry != ft_nullptr);
    FT_ASSERT_EQ(9, found_entry->value);

    ft_map<int, int> assigned;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, assigned.initialize());
    assigned.insert(5, 25);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, assigned.move_from(moved));
    found_entry = assigned.find(3);
    FT_ASSERT(found_entry != ft_nullptr);
    FT_ASSERT_EQ(9, found_entry->value);

    return (1);
}

FT_TEST(test_map_copy_retains_thread_safety)
{
    ft_map<int, int> source;
    ft_map<int, int> copy;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.insert(7, 49);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.copy_from(source));
    Pair<int, int> *copied_entry;

    copied_entry = copy.find(7);
    FT_ASSERT(copied_entry != ft_nullptr);
    FT_ASSERT_EQ(49, copied_entry->value);

    return (1);
}

FT_TEST(test_map_error_queries_follow_lifecycle_contract)
{
    ft_map<int, int> map_value;

    g_map_get_error_returned = FT_FALSE;
    g_map_get_error_result = FT_ERR_SUCCESS;
    g_map_get_error_str_returned = FT_FALSE;
    g_map_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, map_expect_sigabrt(
        map_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_map_get_error_returned);
    FT_ASSERT_EQ(1, map_expect_sigabrt(
        map_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_map_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(map_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}
