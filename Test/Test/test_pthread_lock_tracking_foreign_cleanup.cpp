#include "../test_internal.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/PThread/pthread_lock_tracking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include <thread>

struct s_foreign_cleanup_state
{
    pt_mutex *mutex_pointer;
    int32_t lock_result;
    int32_t unlock_result;
};

static void *foreign_pthread_cleanup_worker(void *argument)
{
    s_foreign_cleanup_state *state;

    state = static_cast<s_foreign_cleanup_state *>(argument);
    state->lock_result = state->mutex_pointer->lock();
    if (state->lock_result == FT_ERR_SUCCESS)
        state->unlock_result = state->mutex_pointer->unlock();
    else
        state->unlock_result = FT_ERR_INVALID_STATE;
    return (ft_nullptr);
}

static void foreign_std_thread_cleanup_worker(s_foreign_cleanup_state *state)
{
    state->lock_result = state->mutex_pointer->lock();
    if (state->lock_result == FT_ERR_SUCCESS)
        state->unlock_result = state->mutex_pointer->unlock();
    else
        state->unlock_result = FT_ERR_INVALID_STATE;
    return ;
}

FT_TEST(test_pt_lock_tracking_direct_pthread_cleanup)
{
    pt_mutex mutex_object;
    s_foreign_cleanup_state state;
    pthread_t thread_identifier;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    ft_size_t baseline_size;
    int32_t error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    baseline_size = thread_infos->size;
    state.mutex_pointer = &mutex_object;
    state.lock_result = FT_ERR_INVALID_STATE;
    state.unlock_result = FT_ERR_INVALID_STATE;
    FT_ASSERT_EQ(0, pthread_create(&thread_identifier, ft_nullptr,
        foreign_pthread_cleanup_worker, &state));
    FT_ASSERT_EQ(0, pthread_join(thread_identifier, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.lock_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.unlock_result);
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(baseline_size, thread_infos->size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}

FT_TEST(test_pt_lock_tracking_std_thread_cleanup)
{
    pt_mutex mutex_object;
    s_foreign_cleanup_state state;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    ft_size_t baseline_size;
    int32_t error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_lock_tracking::notify_thread_exit(THREAD_ID));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.initialize());
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    baseline_size = thread_infos->size;
    state.mutex_pointer = &mutex_object;
    state.lock_result = FT_ERR_INVALID_STATE;
    state.unlock_result = FT_ERR_INVALID_STATE;
    {
        std::thread worker_thread(foreign_std_thread_cleanup_worker, &state);
        worker_thread.join();
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.lock_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.unlock_result);
    error_code = FT_ERR_SUCCESS;
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(baseline_size, thread_infos->size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex_object.destroy());
    return (1);
}
