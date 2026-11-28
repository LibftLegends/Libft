#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_suggestions_register_in_order_and_preserve_entries_on_failure)
{
    rl_clear_suggestions();
    rl_add_suggestion(ft_nullptr);
    FT_ASSERT_EQ(0, suggestion_count);
    char provided[] = "zebra";
    rl_add_suggestion(provided);
    provided[0] = 'x';
    rl_add_suggestion("apple");
    rl_add_suggestion("mango");
    FT_ASSERT_EQ(3, suggestion_count);
    FT_ASSERT_EQ(0, ft_strcmp(suggestions[0], "apple"));
    FT_ASSERT_EQ(0, ft_strcmp(suggestions[1], "mango"));
    FT_ASSERT_EQ(0, ft_strcmp(suggestions[2], "zebra"));

    const char *const mango = suggestions[1];
    rl_add_suggestion("mango");
    cma_set_alloc_limit(1);
    rl_add_suggestion("banana");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(3, suggestion_count);
    FT_ASSERT_EQ(mango, suggestions[1]);
    rl_add_suggestion("banana");
    FT_ASSERT_EQ(4, suggestion_count);
    FT_ASSERT_EQ(0, ft_strcmp(suggestions[1], "banana"));
    FT_ASSERT_EQ(mango, suggestions[2]);
    rl_clear_suggestions();
    FT_ASSERT_EQ(0, suggestion_count);
    FT_ASSERT(suggestions[0] == ft_nullptr && suggestions[3] == ft_nullptr);
    rl_add_suggestion("mango");
    FT_ASSERT_EQ(1, suggestion_count);
    rl_clear_suggestions();
    return (1);
}
