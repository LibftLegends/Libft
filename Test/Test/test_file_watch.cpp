#include "../test_internal.hpp"
#include "../../Modules/File/file_watch.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_watch_event.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#ifndef LIBFT_TEST_BUILD
#endif

static void file_watch_noop_callback(const char *path, int event_type, void *user_data)
{
    (void)path;
    (void)event_type;
    (void)user_data;
    return ;
}

FT_TEST(test_file_watch_error_resets_after_success)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_watch.watch_directory(ft_nullptr, static_cast<file_watch_legacy_callback>(ft_nullptr),
                ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.watch_directory(".",
                &file_watch_noop_callback, ft_nullptr));
    file_watch.stop();
    return (1);
}

FT_TEST(test_file_watch_stop_resets_errno_when_inactive)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    file_watch.stop();
    return (1);
}

FT_TEST(test_file_watch_get_error_resets_errno)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, file_watch.watch_directory(".",
                &file_watch_noop_callback, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.watch_directory(".",
                &file_watch_noop_callback, ft_nullptr));
    file_watch.stop();
    return (1);
}

FT_TEST(test_file_watch_get_error_str_resets_errno)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.destroy());
    return (1);
}

static void file_watch_typed_noop_callback(const char *path,
    file_watch_event_type event_type, void *user_data)
{
    (void)path;
    (void)event_type;
    (void)user_data;
    return ;
}

FT_TEST(test_file_watch_typed_callback_overload_starts)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.watch_directory(".",
            &file_watch_typed_noop_callback, ft_nullptr));
    file_watch.stop();
    return (1);
}

FT_TEST(test_file_watch_debounce_configuration_round_trips)
{
    ft_file_watch file_watch;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.initialize());
    FT_ASSERT_EQ(static_cast<uint32_t>(0), file_watch.get_debounce_milliseconds());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.set_debounce_milliseconds(25));
    FT_ASSERT_EQ(static_cast<uint32_t>(25), file_watch.get_debounce_milliseconds());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_watch.destroy());
    return (1);
}

FT_TEST(test_file_watch_initialize_retries_after_allocation_failure)
{
    ft_file_watch watch;
    cma_set_alloc_limit(1);
    const int32_t initialization_error = watch.initialize();
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, initialization_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.destroy());
    return (1);
}

struct test_file_watch_callback_state
{
    ft_file_watch *watch;
    const char *directory;
    std::atomic<ft_bool> received;
};

static void test_file_watch_stop_from_callback(const char *path, file_watch_event_type,
    void *user_data)
{
    auto *state = static_cast<test_file_watch_callback_state *>(user_data);
    if (ft_strcmp(path, "event.bin") == 0 || ft_strcmp(path, state->directory) == 0)
    {
        state->watch->stop();
        state->received.store(FT_TRUE);
    }
    return ;
}

FT_TEST(test_file_watch_receives_event_stops_from_callback_and_restarts)
{
    ft_string directory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_watch", "dir", &directory));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(directory.c_str(), 0700));
    ft_string *path = file_path_join(directory.c_str(), "event.bin");
    FT_ASSERT(path != ft_nullptr);
    ft_file_watch watch;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.enable_thread_safety());
    test_file_watch_callback_state state{&watch, directory.c_str(), FT_FALSE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.watch_directory(directory.c_str(),
        test_file_watch_stop_from_callback, &state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path->c_str(), "x", 1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (state.received.load() == FT_FALSE && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    watch.stop();
    const int32_t restart_error = watch.watch_directory(directory.c_str(),
        file_watch_typed_noop_callback, ft_nullptr);
    watch.stop();
    const int32_t destroy_error = watch.destroy();
    delete path;
    const int32_t cleanup_error = file_delete_recursive(directory.c_str());
    FT_ASSERT_EQ(FT_TRUE, state.received.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restart_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destroy_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, watch.initialize());
    return (1);
}
