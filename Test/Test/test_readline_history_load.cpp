#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static ft_size_t load_write_count;

static int64_t record_unexpected_load_write(int32_t, const void *, ft_size_t size)
{
    ++load_write_count;
    return (static_cast<int64_t>(size));
}

static int32_t verify_history_load(const char *backend, const char *path, const char *data)
{
    rl_clear_history();
    const ft_size_t data_length = ft_strlen_size_t(data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, data, data_length));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_set_backend(backend, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_TRUE));
    load_write_count = 0;
    su_set_write_syscall_hook(record_unexpected_load_write);
    const int32_t loaded = rl_history_load();
    su_reset_write_syscall_hook();
    ft_string original_file;
    const int32_t original_read = file_read_all(path, original_file);
    const ft_bool correct_history = (history_count == 3
        && ft_strcmp(history[0], "one") == 0 && ft_strcmp(history[1], "") == 0
        && ft_strcmp(history[2], "last") == 0);
    rl_update_history("new command");
    const int32_t disable = rl_history_enable_auto_save(FT_FALSE);
    ft_string saved_file;
    const int32_t saved_read = file_read_all(path, saved_file);
    const int32_t deleted = file_delete(path);
    rl_clear_history();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded);
    FT_ASSERT_EQ(0U, load_write_count);
    FT_ASSERT_EQ(FT_TRUE, correct_history);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_read);
    FT_ASSERT_EQ(data_length, original_file.size());
    FT_ASSERT_EQ(0, ft_memcmp(original_file.c_str(), data, data_length));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, disable);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, saved_read);
    FT_ASSERT(ft_strstr(saved_file.c_str(), "new command") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deleted);
    return (1);
}

FT_TEST(test_readline_plain_history_load_does_not_trigger_auto_save)
{
    return (verify_history_load("plain-text", "test_readline_plain_load.txt", "one\n\nlast"));
}

FT_TEST(test_readline_json_history_load_does_not_trigger_auto_save)
{
    return (verify_history_load("json", "test_readline_json_load.json",
        "{\"history\":{\"0\":\"one\",\"1\":\"\",\"2\":\"last\"}}"));
}
