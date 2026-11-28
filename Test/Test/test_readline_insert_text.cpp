#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_insert_text_accepts_borrowed_prefix_and_suffix)
{
    char buffer[32] = "abcdef";
    readline_state_t state;
    state.buffer = buffer;
    state.buffer_size = sizeof(buffer);
    state.position = 3;
    cma_set_alloc_limit(1);
    const int32_t middle = rl_state_insert_text(&state, buffer + 1);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, middle);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "abcbcdefdef"));
    FT_ASSERT_EQ(8, state.position);
    (void)ft_memcpy(buffer, "abcdef", 7);
    state.position = 6;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_insert_text(&state, buffer + 2));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "abcdefcdef"));
    FT_ASSERT_EQ(10, state.position);
    return (1);
}

FT_TEST(test_readline_insert_text_rebases_borrowed_text_after_growing)
{
    readline_state_t state;
    state.buffer = static_cast<char *>(cma_malloc(7));
    FT_ASSERT(state.buffer != ft_nullptr);
    (void)ft_memcpy(state.buffer, "abcdef", 7);
    state.buffer_size = 7;
    state.position = 1;
    cma_set_alloc_limit(1);
    const int32_t failed = rl_state_insert_text(&state, state.buffer + 3);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(7, state.buffer_size);
    FT_ASSERT_EQ(1, state.position);
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "abcdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_state_insert_text(&state, state.buffer + 3));
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "adefbcdef"));
    FT_ASSERT_EQ(4, state.position);
    state.position = 12;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, rl_state_insert_text(&state, "x"));
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "adefbcdef"));
    cma_free(state.buffer);
    return (1);
}
