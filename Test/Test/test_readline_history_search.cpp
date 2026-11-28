#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_history_search_validates_utf8_and_preserves_direction)
{
    char first[] = "one caf\xC3\xA9";
    char second[] = "two caf\xC3\xA9";
    char malformed[] = "caf\xC3\xA9\xFF";
    char *previous_entries[] = {history[0], history[1], history[2], history[3]};
    const int32_t previous_count = history_count;
    history[0] = first;
    history[1] = ft_nullptr;
    history[2] = second;
    history[3] = malformed;
    history_count = 4;

    int32_t first_match = -1;
    int32_t newest_match = -1;
    int32_t bad_entry_match = 42;
    int32_t bad_query_match = 42;
    int32_t missing_match = 42;
    cma_set_alloc_limit(1);
    const int32_t forward = rl_history_search("\xC3\xA9", -1, FT_FALSE, &first_match);
    const int32_t backward = rl_history_search("caf\xC3\xA9", 2, FT_TRUE, &newest_match);
    const int32_t invalid_entry = rl_history_search("caf", -1, FT_TRUE, &bad_entry_match);
    const int32_t invalid_query = rl_history_search("\xA9", 0, FT_FALSE, &bad_query_match);
    history_count = 3;
    const int32_t missing = rl_history_search("CAF", 1000, FT_FALSE, &missing_match);
    cma_set_alloc_limit(0);

    history[0] = previous_entries[0];
    history[1] = previous_entries[1];
    history[2] = previous_entries[2];
    history[3] = previous_entries[3];
    history_count = previous_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, forward);
    FT_ASSERT_EQ(0, first_match);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backward);
    FT_ASSERT_EQ(2, newest_match);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_entry);
    FT_ASSERT_EQ(-1, bad_entry_match);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_query);
    FT_ASSERT_EQ(-1, bad_query_match);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, missing);
    FT_ASSERT_EQ(-1, missing_match);
    return (1);
}
