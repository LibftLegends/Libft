#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Compatebility/sqlite_support.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/ReadLine/readline.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static char saved_history[8192];
static ft_size_t saved_history_size;

static int64_t capture_short_history_write(int32_t, const void *buffer, ft_size_t length)
{
    if (length > 31)
        length = 31;
    if (length > sizeof(saved_history) - saved_history_size)
        return (-1);
    (void)ft_memcpy(saved_history + saved_history_size, buffer, length);
    saved_history_size += length;
    return (static_cast<int64_t>(length));
}

static int64_t fail_history_write(int32_t, const void *, ft_size_t)
{
    return (-1);
}

FT_TEST(test_readline_history_respects_auto_save_toggle_and_manual_save)
{
    const char *path = "test_readline_history_auto_save.txt";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_set_storage_path(path));
    rl_clear_history();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_TRUE));
    rl_update_history("automatic");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_FALSE));
    rl_update_history("manual");
    ft_string before_manual_save;
    const int32_t read_before = file_read_all(path, before_manual_save);
    const int32_t save_result = rl_history_save();
    ft_string after_manual_save;
    const int32_t read_after = file_read_all(path, after_manual_save);
    const int32_t deleted = file_delete(path);
    rl_clear_history();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_before);
    FT_ASSERT_EQ(0, ft_strcmp(before_manual_save.c_str(), "automatic\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, save_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_after);
    FT_ASSERT_EQ(0, ft_strcmp(after_manual_save.c_str(), "automatic\nmanual\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deleted);
    return (1);
}

#if SQLITE3_AVAILABLE
FT_TEST(test_readline_sqlite_history_round_trip_after_failed_reconfigure)
{
    const char *path = "test_readline_history_sqlite.db";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_set_backend("sqlite", path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_FALSE));
    rl_clear_history();
    char long_entry[4097];
    (void)ft_memset(long_entry, 'x', sizeof(long_entry) - 1);
    long_entry[sizeof(long_entry) - 1] = '\0';
    rl_update_history("caf\xC3\xA9");
    rl_update_history("");
    rl_update_history(long_entry);
    const int32_t reconfigure = rl_history_set_backend("sqlite", "test_readline_missing_history_dir/history.db");
    const int32_t saved = rl_history_save();
    rl_clear_history();
    const int32_t loaded = rl_history_load();
    const ft_bool entries_match = (history_count == 3 && ft_strcmp(history[0], "caf\xC3\xA9") == 0
        && ft_strcmp(history[1], "") == 0 && ft_strcmp(history[2], long_entry) == 0);
    const int32_t closed = rl_history_set_backend("plain-text", "test_readline_history_auto_save.txt");
    const int32_t deleted = file_delete(path);
    rl_clear_history();

    FT_ASSERT_EQ(FT_ERR_SUCCESS, saved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded);
    FT_ASSERT_EQ(FT_TRUE, entries_match);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deleted);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, reconfigure);
    return (1);
}
#endif

FT_TEST(test_readline_plain_history_save_preserves_bytes_with_short_writes)
{
    const char *path = "test_readline_plain_history_save.txt";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_set_storage_path(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_enable_auto_save(FT_FALSE));
    char first[] = "first";
    char long_entry[5001];
    (void)ft_memset(long_entry, 'x', sizeof(long_entry) - 1);
    long_entry[sizeof(long_entry) - 1] = '\0';
    char empty[] = "";
    char last[] = "caf\xC3\xA9";
    char *previous_entries[] = {history[0], history[1], history[2], history[3], history[4]};
    const int32_t previous_count = history_count;
    history[0] = first;
    history[1] = ft_nullptr;
    history[2] = long_entry;
    history[3] = empty;
    history[4] = last;
    history_count = 5;

    saved_history_size = 0;
    su_set_write_syscall_hook(capture_short_history_write);
    const int32_t saved = rl_history_save();
    su_reset_write_syscall_hook();
    su_set_write_syscall_hook(fail_history_write);
    const int32_t failed = rl_history_save();
    su_reset_write_syscall_hook();
    const int32_t written = rl_history_save();
    ft_string file;
    const int32_t read_back = file_read_all(path, file);
    const int32_t deleted = file_delete(path);

    history[0] = previous_entries[0];
    history[1] = previous_entries[1];
    history[2] = previous_entries[2];
    history[3] = previous_entries[3];
    history[4] = previous_entries[4];
    history_count = previous_count;
    const char first_line[] = "first\n";
    const char last_lines[] = "\n\ncaf\xC3\xA9\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, saved);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, failed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, written);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_back);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deleted);
    FT_ASSERT_EQ(sizeof(first_line) - 1 + sizeof(long_entry) - 1 + sizeof(last_lines) - 1,
        saved_history_size);
    FT_ASSERT_EQ(0, ft_memcmp(saved_history, first_line, sizeof(first_line) - 1));
    FT_ASSERT_EQ(0, ft_memcmp(saved_history + sizeof(first_line) - 1, long_entry, sizeof(long_entry) - 1));
    FT_ASSERT_EQ(0, ft_memcmp(saved_history + sizeof(first_line) + sizeof(long_entry) - 2,
        last_lines, sizeof(last_lines) - 1));
    FT_ASSERT_EQ(saved_history_size, file.size());
    FT_ASSERT_EQ(0, ft_memcmp(saved_history, file.c_str(), file.size()));
    return (1);
}

FT_TEST(test_readline_json_history_save_preserves_numeric_keys_order_and_escapes)
{
    const char *path = "test_readline_json_history_save.json";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_history_set_backend("json", path));
    constexpr int32_t ENTRY_COUNT = 11;
    char ordinary[] = "sample";
    char escaped[] = "quoted \"text\"\nline";
    char long_entry[513];
    (void)ft_memset(long_entry, 'x', sizeof(long_entry) - 1);
    long_entry[sizeof(long_entry) - 1] = '\0';
    char *previous_entries[ENTRY_COUNT];
    int32_t index = 0;
    const int32_t previous_count = history_count;
    while (index < ENTRY_COUNT)
    {
        previous_entries[index] = history[index];
        history[index] = ordinary;
        ++index;
    }
    history[0] = escaped;
    history[1] = ft_nullptr;
    history[10] = long_entry;
    history_count = ENTRY_COUNT;

    const int32_t stored = rl_history_save();
    ft_string initial;
    const int32_t initial_read = file_read_all(path, initial);
    cma_set_alloc_limit(256);
    const int32_t failed_save = rl_history_save();
    cma_set_alloc_limit(0);
    ft_string retained;
    const int32_t final_read = file_read_all(path, retained);
    json_group *root = json_read_from_file(path);
    const int32_t deleted = file_delete(path);
    index = 0;
    while (index < ENTRY_COUNT)
    {
        history[index] = previous_entries[index];
        ++index;
    }
    history_count = previous_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, failed_save);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initial_read);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, final_read);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deleted);
    FT_ASSERT(initial == retained);
    FT_ASSERT(root != ft_nullptr);
    const json_group *group = json_find_group(root, "history");
    FT_ASSERT(group != ft_nullptr);
    const json_item *item = group->items;
    index = 0;
    while (index < ENTRY_COUNT)
    {
        FT_ASSERT(item != ft_nullptr);
        if (index < 10)
        {
            FT_ASSERT_EQ(static_cast<char>('0' + index), item->key[0]);
            FT_ASSERT_EQ('\0', item->key[1]);
        }
        else
            FT_ASSERT_EQ(0, ft_strcmp(item->key, "10"));
        const char *expected_value = ordinary;
        if (index == 0)
            expected_value = escaped;
        else if (index == 1)
            expected_value = "";
        else if (index == 10)
            expected_value = long_entry;
        FT_ASSERT_EQ(0, ft_strcmp(item->value, expected_value));
        item = item->next;
        ++index;
    }
    FT_ASSERT(item == ft_nullptr);
    json_free_groups(root);
    return (1);
}
