#include "../test_internal.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include <cstdlib>
#include <windows.h>

FT_TEST(test_readline_raw_mode_repeated_enable_preserves_console_settings)
{
    if (std::getenv("FT_TEST_READLINE_REAL_CONSOLE") == ft_nullptr)
        return (1);
    readline_state_t state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_prepare_thread_safety(&state));
    const HANDLE previous_input = GetStdHandle(STD_INPUT_HANDLE);
    const HANDLE input = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, ft_nullptr, OPEN_EXISTING, 0, ft_nullptr);
    if (input == INVALID_HANDLE_VALUE)
    {
        rl_state_teardown_thread_safety(&state);
        FT_ASSERT(input != INVALID_HANDLE_VALUE);
    }
    DWORD original_mode = 0;
    if (GetConsoleMode(input, &original_mode) == 0)
    {
        (void)CloseHandle(input);
        rl_state_teardown_thread_safety(&state);
        FT_ASSERT(FT_FALSE);
    }
    const DWORD initial_mode = original_mode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT;
    const ft_bool configured = (SetConsoleMode(input, initial_mode) != 0
        && SetStdHandle(STD_INPUT_HANDLE, input) != 0);
    ft_bool first_activation = FT_FALSE;
    ft_bool second_activation = FT_TRUE;
    DWORD active_mode = 0;
    DWORD restored_mode = 0;
    int32_t first_result = FT_ERR_INTERNAL;
    int32_t second_result = FT_ERR_INTERNAL;
    int32_t initialization_result = FT_ERR_INTERNAL;
    int32_t disable_result = FT_ERR_INTERNAL;
    if (configured == FT_TRUE)
    {
        first_result = rl_enable_raw_mode(&first_activation);
        second_result = rl_enable_raw_mode(&second_activation);
        cma_set_alloc_limit(1);
        initialization_result = rl_initialize_state(&state);
        cma_set_alloc_limit(0);
        (void)GetConsoleMode(input, &active_mode);
        disable_result = rl_disable_raw_mode();
        (void)GetConsoleMode(input, &restored_mode);
    }
    (void)SetConsoleMode(input, original_mode);
    (void)SetStdHandle(STD_INPUT_HANDLE, previous_input);
    (void)CloseHandle(input);
    rl_state_teardown_thread_safety(&state);

    FT_ASSERT_EQ(FT_TRUE, configured);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_result);
    FT_ASSERT_EQ(FT_TRUE, first_activation);
    FT_ASSERT_EQ(FT_FALSE, second_activation);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, initialization_result);
    FT_ASSERT_EQ(initial_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT), active_mode);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, disable_result);
    FT_ASSERT_EQ(initial_mode, restored_mode);
    return (1);
}
#endif
