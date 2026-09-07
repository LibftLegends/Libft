#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../File/file_watch.hpp"

class pt_mutex;

struct config_entry
{
    pt_mutex *mutex;
    char    *section;
    char    *key;
    char    *value;
};

struct config_data
{
    config_entry  *entries;
    ft_size_t            entry_count;
    pt_mutex             *mutex;
};

config_data   *config_data_create();
int32_t       config_data_prepare_thread_safety(config_data *config);
/* Teardown requires exclusive ownership of the config and all entry users. */
void        config_data_teardown_thread_safety(config_data *config);
config_data   *config_parse(const char *filename);
void        config_data_free(config_data *config);
config_data   *config_load_env();
config_data   *config_load_file(const char *filename);
config_data   *config_reload_file(const char *filename);
int32_t       config_write_file(const config_data *config, const char *filename);
int32_t       config_save_file(const config_data *config, const char *filename);
config_data   *config_merge(const config_data *base_config, const config_data *override_config);
ft_file_watch *config_watch_file(const char *filename, file_watch_callback callback, void *user_data);

int32_t       config_entry_prepare_thread_safety(config_entry *entry);
/* Teardown requires exclusive ownership of the entry and its users. */
void        config_entry_teardown_thread_safety(config_entry *entry);
int32_t       config_entry_lock(config_entry *entry, ft_bool *lock_acquired);
void        config_entry_unlock(config_entry *entry, ft_bool lock_acquired);

#endif
