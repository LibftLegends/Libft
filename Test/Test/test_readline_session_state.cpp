#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_reinitialize_reuses_buffer_and_preserves_state_on_failure)
{
    readline_state_t state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_initialize_state(&state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_disable_raw_mode());
    char *const existing_buffer = state.buffer;
    (void)ft_memcpy(state.buffer, "previous", 9);
    state.position = 8;
    cma_set_alloc_limit(1);
    const int32_t reuse_result = rl_initialize_state(&state);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reuse_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_disable_raw_mode());
    FT_ASSERT_EQ(existing_buffer, state.buffer);
    FT_ASSERT_EQ('\0', state.buffer[0]);
    FT_ASSERT_EQ(0, state.position);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_resize_buffer(&state.buffer, &state.buffer_size, INITIAL_BUFFER_SIZE + 1));
    char *const oversized_buffer = state.buffer;
    (void)ft_memcpy(state.buffer, "retained", 9);
    state.position = 8;
    cma_set_alloc_limit(1);
    const int32_t resize_result = rl_initialize_state(&state);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, resize_result);
    FT_ASSERT_EQ(oversized_buffer, state.buffer);
    FT_ASSERT_EQ(INITIAL_BUFFER_SIZE + 1, state.buffer_size);
    FT_ASSERT_EQ(8, state.position);
    FT_ASSERT_EQ(0, ft_strcmp(state.buffer, "retained"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_initialize_state(&state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_disable_raw_mode());
    FT_ASSERT_EQ(INITIAL_BUFFER_SIZE, state.buffer_size);
    FT_ASSERT_EQ(0, state.position);
    cma_free(state.buffer);
    rl_state_teardown_thread_safety(&state);
    return (1);
}

FT_TEST(test_readline_completion_reset_releases_dynamic_matches_and_can_rank_again)
{
    rl_clear_suggestions();
    rl_add_suggestion("apple");
    char buffer[16] = "ap";
    readline_state_t state;
    state.buffer = buffer;
    state.buffer_size = sizeof(buffer);
    state.position = 2;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_handle_tab_completion(&state, "> "));
    FT_ASSERT_EQ(1, state.current_match_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_completion_add_candidate("temporary"));
    rl_reset_completion_mode(&state);
    FT_ASSERT_EQ(0, state.current_match_count);
    FT_ASSERT_EQ(0, state.current_match_index);
    FT_ASSERT_EQ(0, state.in_completion_mode);
    int32_t dynamic_count = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_completion_get_dynamic_count(&dynamic_count));
    FT_ASSERT_EQ(0, dynamic_count);
    (void)ft_memcpy(buffer, "ap", 3);
    state.position = 2;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_handle_tab_completion(&state, "> "));
    FT_ASSERT_EQ(1, state.current_match_count);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "apple"));
    rl_clear_suggestions();
    return (1);
}
