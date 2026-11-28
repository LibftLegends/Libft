#include "../test_internal.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Advanced/advanced.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static void test_readline_cleanup_state(readline_state_t *state)
{
    ft_bool lock_acquired;
    int32_t lock_result;

    if (state == ft_nullptr)
        return ;
    lock_acquired = FT_FALSE;
    lock_result = rl_state_lock(state, &lock_acquired);
    if (lock_result == 0 && state->buffer != ft_nullptr)
    {
        cma_free(state->buffer);
        state->buffer = ft_nullptr;
    }
    if (lock_result == 0)
    {
        if (lock_acquired == FT_TRUE)
            rl_state_unlock(state, lock_acquired);
    }
    rl_state_teardown_thread_safety(state);
    return ;
}

static int test_readline_custom_key_callback(readline_state_t *state, const char *prompt, void *user_data)
{
    int *counter_pointer;

    (void)state;
    (void)prompt;
    counter_pointer = static_cast<int *>(user_data);
    if (counter_pointer != ft_nullptr)
        *counter_pointer += 1;
    return (0);
}

FT_TEST(test_readline_custom_key_bindings_dispatch)
{
    readline_state_t state;
    int initialize_result;
    int bind_result;
    int dispatch_result;
    int unbind_result;
    int callback_counter;
    ft_bool key_handled;

    ft_bzero(&state, sizeof(state));
    callback_counter = 0;
    initialize_result = rl_initialize_state(&state);
    if (initialize_result != 0)
        return (0);
    bind_result = rl_bind_key('x', test_readline_custom_key_callback, &callback_counter);
    rl_disable_raw_mode();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bind_result);
    key_handled = FT_FALSE;
    dispatch_result = rl_dispatch_custom_key(&state, "> ", 'x', &key_handled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dispatch_result);
    FT_ASSERT_EQ(FT_TRUE, key_handled);
    FT_ASSERT_EQ(1, callback_counter);
    callback_counter = 0;
    unbind_result = rl_unbind_key('x');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unbind_result);
    key_handled = FT_FALSE;
    dispatch_result = rl_dispatch_custom_key(&state, "> ", 'x', &key_handled);
    FT_ASSERT_EQ(0, dispatch_result);
    FT_ASSERT_EQ(FT_FALSE, key_handled);
    FT_ASSERT_EQ(0, callback_counter);
    test_readline_cleanup_state(&state);
    return (1);
}

FT_TEST(test_readline_custom_key_bindings_update_and_remove_signed_keys)
{
    readline_state_t state;
    int32_t previous = 0;
    int32_t updated = 0;
    const int32_t keys[] = {FT_INT32_MAX, -500, FT_INT32_MIN, 500};
    ft_size_t index = 0;
    while (index < sizeof(keys) / sizeof(keys[0]))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_bind_key(keys[index], test_readline_custom_key_callback, &previous));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_bind_key(-500, test_readline_custom_key_callback, &updated));
    ft_bool handled = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_dispatch_custom_key(&state, "", -500, &handled));
    FT_ASSERT_EQ(FT_TRUE, handled);
    FT_ASSERT_EQ(1, updated);
    FT_ASSERT_EQ(0, previous);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_unbind_key(-500));
    FT_ASSERT_EQ(FT_ERR_INTERNAL, rl_unbind_key(-500));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_dispatch_custom_key(&state, "", -500, &handled));
    FT_ASSERT_EQ(FT_FALSE, handled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_dispatch_custom_key(&state, "", FT_INT32_MIN, &handled));
    FT_ASSERT_EQ(FT_TRUE, handled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_dispatch_custom_key(&state, "", FT_INT32_MAX, &handled));
    FT_ASSERT_EQ(FT_TRUE, handled);
    FT_ASSERT_EQ(2, previous);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_unbind_key(FT_INT32_MIN));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_unbind_key(FT_INT32_MAX));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_unbind_key(500));
    return (1);
}
