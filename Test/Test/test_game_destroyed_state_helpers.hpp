#ifndef TEST_GAME_DESTROYED_STATE_HELPERS_HPP
#define TEST_GAME_DESTROYED_STATE_HELPERS_HPP

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../test_internal.hpp"
#include <csetjmp>
#include <csignal>
#include <cstring>
#include <new>
#if !defined(_WIN32) && !defined(_WIN64)
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

static volatile sig_atomic_t g_game_destroyed_signal_caught = 0;
static sigjmp_buf g_game_destroyed_jump_buffer;

static void __attribute__((unused)) game_destroyed_signal_handler(int signal_value)
{
    g_game_destroyed_signal_caught = signal_value;
    siglongjmp(g_game_destroyed_jump_buffer, 1);
    return ;
}

#if !defined(_WIN32) && !defined(_WIN64)
template <typename TypeName>
static int32_t game_destroyed_expect_sigabrt_process(
    void (*operation)(TypeName &))
{
    pid_t child_process_id;
    pid_t waited_process_id;
    int child_status;
    TypeName object_instance;

    child_process_id = fork();
    if (child_process_id < 0)
        return (0);
    if (child_process_id == 0)
    {
        (void)signal(SIGABRT, SIG_DFL);
        if (SIGIOT != SIGABRT)
            (void)signal(SIGIOT, SIG_DFL);
        if (object_instance.initialize() != FT_ERR_SUCCESS)
            _exit(0);
        if (object_instance.destroy() != FT_ERR_SUCCESS)
            _exit(0);
        operation(object_instance);
        _exit(0);
    }
    waited_process_id = waitpid(child_process_id, &child_status, 0);
    if (waited_process_id != child_process_id)
        return (0);
    if (WIFSIGNALED(child_status) == 0)
        return (0);
    if (WTERMSIG(child_status) == SIGABRT)
        return (1);
    if (SIGIOT != SIGABRT && WTERMSIG(child_status) == SIGIOT)
        return (1);
    return (0);
}
#endif

template <typename TypeName>
static int32_t expect_game_destroyed_sigabrt(void (*operation)(TypeName &))
{
#if !defined(_WIN32) && !defined(_WIN64)
    return (game_destroyed_expect_sigabrt_process<TypeName>(operation));
#else
    struct sigaction old_action_abort;
    struct sigaction new_action_abort;
    struct sigaction old_action_iot;
    struct sigaction new_action_iot;
    alignas(TypeName) unsigned char object_storage[sizeof(TypeName)];
    TypeName *object_pointer;
    volatile int32_t jump_result;
    ft_bool iot_handler_installed;
    volatile int32_t result;

    iot_handler_installed = FT_FALSE;
    result = 0;
    std::memset(&old_action_abort, 0, sizeof(old_action_abort));
    std::memset(&new_action_abort, 0, sizeof(new_action_abort));
    std::memset(&old_action_iot, 0, sizeof(old_action_iot));
    std::memset(&new_action_iot, 0, sizeof(new_action_iot));
    new_action_abort.sa_handler = &game_destroyed_signal_handler;
    new_action_iot.sa_handler = &game_destroyed_signal_handler;
    sigemptyset(&new_action_abort.sa_mask);
    sigemptyset(&new_action_iot.sa_mask);
    if (sigaction(SIGABRT, &new_action_abort, &old_action_abort) != 0)
        return (0);
    if (SIGIOT != SIGABRT)
    {
        if (sigaction(SIGIOT, &new_action_iot, &old_action_iot) != 0)
        {
            (void)sigaction(SIGABRT, &old_action_abort, ft_nullptr);
            return (0);
        }
        iot_handler_installed = FT_TRUE;
    }
    std::memset(object_storage, 0, sizeof(object_storage));
    object_pointer = new (object_storage) TypeName();
    if (object_pointer->initialize() != FT_ERR_SUCCESS)
        goto cleanup;
    if (object_pointer->destroy() != FT_ERR_SUCCESS)
        goto cleanup;
    g_game_destroyed_signal_caught = 0;
    jump_result = sigsetjmp(g_game_destroyed_jump_buffer, 1);
    if (jump_result == 0)
        operation(*object_pointer);
    if (g_game_destroyed_signal_caught == SIGABRT ||
        (SIGIOT != SIGABRT && g_game_destroyed_signal_caught == SIGIOT))
        result = 1;
cleanup:
    if (iot_handler_installed == FT_TRUE)
        (void)sigaction(SIGIOT, &old_action_iot, ft_nullptr);
    (void)sigaction(SIGABRT, &old_action_abort, ft_nullptr);
    object_pointer->~TypeName();
    return (result);
#endif
}

#endif
