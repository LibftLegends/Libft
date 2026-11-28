#include "../test_internal.hpp"
#include "../../Modules/DUMB/render_window.hpp"
#include "../../Modules/DUMB/sound_device.hpp"
#include "../../Modules/DUMB/sound_clip.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <csetjmp>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

class test_sound_device_lifecycle_impl : public ft_sound_device
{
    public:
        test_sound_device_lifecycle_impl(void);
        test_sound_device_lifecycle_impl(const test_sound_device_lifecycle_impl &other);
        test_sound_device_lifecycle_impl(test_sound_device_lifecycle_impl &&other);
        test_sound_device_lifecycle_impl &operator=(
            const test_sound_device_lifecycle_impl &other) = delete;
        test_sound_device_lifecycle_impl &operator=(
            test_sound_device_lifecycle_impl &&other) = delete;
        ~test_sound_device_lifecycle_impl(void);

        int32_t open(const ft_sound_spec *spec);
        void close(void);
        void pause(int32_t pause_on);
        void play(const ft_sound_clip *clip);
        void stop(void);
};

test_sound_device_lifecycle_impl::test_sound_device_lifecycle_impl(void)
{
    return ;
}

test_sound_device_lifecycle_impl::test_sound_device_lifecycle_impl(
    const test_sound_device_lifecycle_impl &other)
{
    (void)this->initialize(other);
    return ;
}

test_sound_device_lifecycle_impl::test_sound_device_lifecycle_impl(
    test_sound_device_lifecycle_impl &&other)
{
    (void)this->initialize(static_cast<ft_sound_device &&>(other));
    return ;
}

test_sound_device_lifecycle_impl::~test_sound_device_lifecycle_impl(void)
{
    return ;
}

int32_t test_sound_device_lifecycle_impl::open(const ft_sound_spec *spec)
{
    (void)spec;
    return (FT_ERR_SUCCESS);
}

void test_sound_device_lifecycle_impl::close(void)
{
    return ;
}

void test_sound_device_lifecycle_impl::pause(int32_t pause_on)
{
    (void)pause_on;
    return ;
}

void test_sound_device_lifecycle_impl::play(const ft_sound_clip *clip)
{
    (void)clip;
    return ;
}

void test_sound_device_lifecycle_impl::stop(void)
{
    return ;
}

static sigjmp_buf g_dumb_lifecycle_jump_buffer;
static volatile sig_atomic_t g_dumb_lifecycle_signal;
static ft_sound_clip g_sound_clip_abort_instance;

static void dumb_lifecycle_sigabrt_handler(int32_t signal_number)
{
    g_dumb_lifecycle_signal = signal_number;
    siglongjmp(g_dumb_lifecycle_jump_buffer, 1);
}

static int32_t dumb_expect_sigabrt(void (*operation)())
{
    struct sigaction new_action;
    struct sigaction old_action;
    int32_t result;
    int32_t saved_stderr;
    int32_t pipe_read_descriptor;
    int32_t pipe_write_descriptor;
    char output_buffer[8192];

    std::memset(&new_action, 0, sizeof(new_action));
    std::memset(&old_action, 0, sizeof(old_action));
    new_action.sa_handler = dumb_lifecycle_sigabrt_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    if (sigaction(SIGABRT, &new_action, &old_action) != 0)
        return (0);
    if (test_capture_abort_output_begin(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor) == 0)
    {
        if (sigaction(SIGABRT, &old_action, ft_nullptr) != 0)
            return (0);
        return (0);
    }

    g_dumb_lifecycle_signal = 0;
    if (sigsetjmp(g_dumb_lifecycle_jump_buffer, 1) == 0)
    {
        operation();
        result = 0;
    }
    else
    {
        result = (g_dumb_lifecycle_signal == SIGABRT);
    }
    if (test_capture_abort_output_end(saved_stderr, pipe_read_descriptor,
            pipe_write_descriptor, output_buffer, sizeof(output_buffer)) == 0)
        return (0);

    if (sigaction(SIGABRT, &old_action, ft_nullptr) != 0)
        return (0);
    return (result);
}

static void sound_device_initialize_twice_aborts_operation()
{
    test_sound_device_lifecycle_impl sound_device_instance;

    (void)sound_device_instance.initialize();
    (void)sound_device_instance.initialize();
    return ;
}

static void sound_clip_initialize_twice_aborts_operation()
{
    (void)g_sound_clip_abort_instance.initialize();
    (void)g_sound_clip_abort_instance.initialize();
    return ;
}

static void render_window_initialize_twice_aborts_operation()
{
    ft_render_window render_window_instance;

    (void)render_window_instance.initialize();
    (void)render_window_instance.initialize();
    return ;
}

static void sound_device_initialize_copy_destroyed_source_succeeds_as_destroyed()
{
    test_sound_device_lifecycle_impl source_instance;
    test_sound_device_lifecycle_impl destination_copy;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_copy.initialize(source_instance);
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_copy._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

static void sound_device_initialize_move_destroyed_source_succeeds_as_destroyed()
{
    test_sound_device_lifecycle_impl source_instance;
    test_sound_device_lifecycle_impl destination_move;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_move.initialize(
        static_cast<test_sound_device_lifecycle_impl &&>(source_instance));
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_move._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

static void sound_clip_initialize_copy_destroyed_source_succeeds_as_destroyed()
{
    ft_sound_clip source_instance;
    ft_sound_clip destination_copy;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_copy.initialize(source_instance);
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_copy._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

static void sound_clip_initialize_move_destroyed_source_succeeds_as_destroyed()
{
    ft_sound_clip source_instance;
    ft_sound_clip destination_move;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_move.initialize(
        static_cast<ft_sound_clip &&>(source_instance));
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_move._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

static void render_window_initialize_copy_destroyed_source_succeeds_as_destroyed()
{
    ft_render_window source_instance;
    ft_render_window destination_copy;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_copy.initialize(source_instance);
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_copy._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

static void render_window_initialize_move_destroyed_source_succeeds_as_destroyed()
{
    ft_render_window source_instance;
    ft_render_window destination_move;
    int32_t initialize_error;

    (void)source_instance.initialize();
    (void)source_instance.destroy();
    initialize_error = destination_move.initialize(
        static_cast<ft_render_window &&>(source_instance));
    if (initialize_error != FT_ERR_SUCCESS)
        abort();
    if (destination_move._initialised_state != FT_CLASS_STATE_DESTROYED)
        abort();
    return ;
}

FT_TEST(test_dumb_sound_device_initialize_destroy_reinitialize_success)
{
    test_sound_device_lifecycle_impl sound_device_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_copy_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        sound_device_initialize_copy_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_move_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        sound_device_initialize_move_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_sound_device_move_self_initialised_is_noop)
{
    test_sound_device_lifecycle_impl sound_device_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.move(sound_device_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_copy_self_initialised_is_noop)
{
    test_sound_device_lifecycle_impl sound_device_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize(sound_device_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_move_self_initialised_is_noop)
{
    test_sound_device_lifecycle_impl sound_device_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        sound_device_instance.initialize(static_cast<test_sound_device_lifecycle_impl &&>(
            sound_device_instance)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_move_uninitialised_destination_succeeds)
{
    test_sound_device_lifecycle_impl source_instance;
    test_sound_device_lifecycle_impl destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_move_destroyed_destination_succeeds)
{
    test_sound_device_lifecycle_impl source_instance;
    test_sound_device_lifecycle_impl destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_move_transfers_mutex)
{
    test_sound_device_lifecycle_impl source;
    test_sound_device_lifecycle_impl destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source._mutex;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(static_cast<ft_sound_device &&>(source)));
    FT_ASSERT(destination._mutex == source_mutex);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT(source._mutex == ft_nullptr);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    return (1);
}

FT_TEST(test_dumb_sound_device_move_lock_failure_preserves_source)
{
    test_sound_device_lifecycle_impl source;
    test_sound_device_lifecycle_impl destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source._mutex;

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(destination._mutex == ft_nullptr);
    FT_ASSERT(source._mutex == source_mutex);
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    return (1);
}

FT_TEST(test_dumb_sound_device_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, dumb_expect_sigabrt(sound_device_initialize_twice_aborts_operation));
    return (1);
}

FT_TEST(test_dumb_sound_device_destroy_twice_returns_invalid_state)
{
    test_sound_device_lifecycle_impl sound_device_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_device_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_destroy_reinitialize_success)
{
    ft_sound_clip sound_clip_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_copy_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        sound_clip_initialize_copy_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_move_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        sound_clip_initialize_move_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_sound_clip_move_self_initialised_is_noop)
{
    ft_sound_clip sound_clip_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.move(sound_clip_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_copy_self_initialised_is_noop)
{
    ft_sound_clip sound_clip_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize(sound_clip_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_move_self_initialised_is_noop)
{
    ft_sound_clip sound_clip_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        sound_clip_instance.initialize(static_cast<ft_sound_clip &&>(sound_clip_instance)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_move_uninitialised_destination_succeeds)
{
    ft_sound_clip source_instance;
    ft_sound_clip destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_move_destroyed_destination_succeeds)
{
    ft_sound_clip source_instance;
    ft_sound_clip destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, dumb_expect_sigabrt(sound_clip_initialize_twice_aborts_operation));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, g_sound_clip_abort_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_sound_clip_destroy_twice_returns_invalid_state)
{
    ft_sound_clip sound_clip_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sound_clip_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_destroy_reinitialize_success)
{
    ft_render_window render_window_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_copy_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        render_window_initialize_copy_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_move_from_destroyed_source_succeeds)
{
    FT_ASSERT_EQ(0, dumb_expect_sigabrt(
        render_window_initialize_move_destroyed_source_succeeds_as_destroyed));
    return (1);
}

FT_TEST(test_dumb_render_window_move_self_initialised_is_noop)
{
    ft_render_window render_window_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.move(render_window_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_copy_self_initialised_is_noop)
{
    ft_render_window render_window_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        render_window_instance.initialize(render_window_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_move_self_initialised_is_noop)
{
    ft_render_window render_window_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        render_window_instance.initialize(static_cast<ft_render_window &&>(
            render_window_instance)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_move_uninitialised_destination_succeeds)
{
    ft_render_window source_instance;
    ft_render_window destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_move_destroyed_destination_succeeds)
{
    ft_render_window source_instance;
    ft_render_window destination_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.move(source_instance));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_instance.destroy());
    return (1);
}

FT_TEST(test_dumb_render_window_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, dumb_expect_sigabrt(render_window_initialize_twice_aborts_operation));
    return (1);
}

FT_TEST(test_dumb_render_window_destroy_twice_returns_invalid_state)
{
    ft_render_window render_window_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, render_window_instance.destroy());
    return (1);
}
