#include "config.hpp"
#include "../Errno/errno.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Advanced/advanced.hpp"
#include "../File/file_utils.hpp"
#include "../JSon/json.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t config_mutex_lock(pt_mutex *mutex, ft_bool *lock_acquired)
{
    int32_t lock_result;

    lock_result = pt_mutex_lock_if_not_null(mutex);
    if (lock_result == FT_ERR_SUCCESS && lock_acquired)
    {
        if (mutex != ft_nullptr)
            *lock_acquired = FT_TRUE;
    }
    return (lock_result);
}

static int32_t config_lock_if_enabled(config_data *config, ft_bool *lock_acquired)
{
    if (!config || !config->mutex)
        return (FT_ERR_SUCCESS);
    return (config_mutex_lock(config->mutex, lock_acquired));
}
static int32_t config_mutex_create(pt_mutex **mutex_pointer)
{
    if (!mutex_pointer)
        return (FT_ERR_INVALID_ARGUMENT);
    if (*mutex_pointer != ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_mutex *mutex = new (std::nothrow) pt_mutex();
    if (!mutex)
        return (FT_ERR_NO_MEMORY);
    int32_t initialize_error = mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex;
        return (initialize_error);
    }
    *mutex_pointer = mutex;
    return (FT_ERR_SUCCESS);
}

static void config_mutex_destroy(pt_mutex **mutex_pointer)
{
    if (!mutex_pointer || *mutex_pointer == ft_nullptr)
        return ;
    pt_mutex *mutex = *mutex_pointer;
    mutex->destroy();
    delete mutex;
    *mutex_pointer = ft_nullptr;
    return ;
}

static void config_unlock_guard(config_data *config, ft_bool lock_acquired)
{
    if (!config || !config->mutex || lock_acquired == FT_FALSE)
        return ;
    (void)pt_mutex_unlock_if_not_null(config->mutex);
    return ;
}

config_data *config_data_create()
{
    config_data *config;

    config = static_cast<config_data*>(adv_calloc(1, sizeof(config_data)));
    if (!config)
    {
        return (ft_nullptr);
    }
    if (config_data_prepare_thread_safety(config) != FT_ERR_SUCCESS)
    {
        cma_free(config);
        return (ft_nullptr);
    }
    return (config);
}

int32_t config_data_prepare_thread_safety(config_data *config)
{
    if (!config)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (config->mutex)
    {
        return (FT_ERR_SUCCESS);
    }
    int32_t mutex_error = config_mutex_create(&config->mutex);
    if (mutex_error != FT_ERR_SUCCESS)
    {
        return (mutex_error);
    }
    return (FT_ERR_SUCCESS);
}

void config_data_teardown_thread_safety(config_data *config)
{
    if (!config)
        return ;
    config_mutex_destroy(&config->mutex);
    return ;
}

static char *trim_whitespace(char *string)
{
    if (!string)
        return (string);
    while (*string && ft_isspace(static_cast<int32_t>(
                static_cast<uint8_t>(*string))))
        string++;
    char *end_pointer = string + ft_strlen(string);
    while (end_pointer > string && ft_isspace(static_cast<int32_t>(
                static_cast<uint8_t>(end_pointer[-1]))))
        end_pointer--;
    *end_pointer = '\0';
    return (string);
}

void config_data_free(config_data *config)
{
    if (!config)
    {
        return ;
    }
    ft_bool mutex_locked;
    ft_size_t entry_index;
    ft_bool already_owned;
    int32_t lock_result;

    mutex_locked = FT_FALSE;
    already_owned = FT_FALSE;
    lock_result = config_lock_if_enabled(config, &mutex_locked);
    if (lock_result != FT_ERR_SUCCESS)
    {
        if (lock_result == FT_ERR_MUTEX_ALREADY_LOCKED && config->mutex)
        {
            already_owned = FT_TRUE;
        }
        else
        {
            config_data_teardown_thread_safety(config);
            cma_free(config);
            return ;
        }
    }
    entry_index = 0;
    while (entry_index < config->entry_count)
    {
        cma_free(config->entries[entry_index].section);
        cma_free(config->entries[entry_index].key);
        cma_free(config->entries[entry_index].value);
        config_entry_teardown_thread_safety(&config->entries[entry_index]);
        ++entry_index;
    }
    cma_free(config->entries);
    if (already_owned == FT_TRUE)
        (void)pt_mutex_unlock_if_not_null(config->mutex);
    else
        config_unlock_guard(config, mutex_locked);
    config_data_teardown_thread_safety(config);
    cma_free(config);
    return ;
}

static int32_t config_parse_line(config_data *config, char *line_string,
    char **current_section)
{
    char *equals_sign;
    char *key;
    char *value;
    char *key_start;
    char *value_start;
    config_entry *new_entries;
    config_entry *new_entry;
    int32_t error_code;

    if (!config || !line_string || !current_section)
        return (FT_ERR_INVALID_ARGUMENT);
    line_string = trim_whitespace(line_string);
    if (*line_string == '\0' || *line_string == ';'
        || *line_string == '#')
        return (FT_ERR_SUCCESS);
    if (*line_string == '[')
    {
        char *closing_bracket = ft_strchr(line_string, ']');
        if (!closing_bracket)
            return (FT_ERR_INVALID_ARGUMENT);
        *closing_bracket = '\0';
        cma_free(*current_section);
        *current_section = ft_nullptr;
        if (*(line_string + 1) != '\0')
        {
            *current_section = adv_strdup(line_string + 1);
            if (!*current_section)
                return (FT_ERR_NO_MEMORY);
        }
        return (FT_ERR_SUCCESS);
    }
    key = ft_nullptr;
    value = ft_nullptr;
    equals_sign = ft_strchr(line_string, '=');
    if (equals_sign)
    {
        *equals_sign = '\0';
        key_start = trim_whitespace(line_string);
        value_start = trim_whitespace(equals_sign + 1);
        if (*key_start != '\0')
        {
            key = adv_strdup(key_start);
            if (!key)
                return (FT_ERR_NO_MEMORY);
        }
        if (*value_start != '\0')
        {
            value = adv_strdup(value_start);
            if (!value)
            {
                cma_free(key);
                return (FT_ERR_NO_MEMORY);
            }
        }
    }
    else
    {
        key_start = trim_whitespace(line_string);
        if (*key_start != '\0')
        {
            key = adv_strdup(key_start);
            if (!key)
                return (FT_ERR_NO_MEMORY);
        }
    }
    new_entries = static_cast<config_entry*>(cma_realloc(config->entries,
        sizeof(config_entry) * (config->entry_count + 1U)));
    if (!new_entries)
    {
        cma_free(key);
        cma_free(value);
        return (FT_ERR_NO_MEMORY);
    }
    config->entries = new_entries;
    new_entry = &config->entries[config->entry_count];
    new_entry->mutex = ft_nullptr;
    new_entry->section = ft_nullptr;
    new_entry->key = key;
    new_entry->value = value;
    if (*current_section)
    {
        new_entry->section = adv_strdup(*current_section);
        if (!new_entry->section)
        {
            cma_free(new_entry->key);
            cma_free(new_entry->value);
            new_entry->key = ft_nullptr;
            new_entry->value = ft_nullptr;
            return (FT_ERR_NO_MEMORY);
        }
    }
    error_code = config_entry_prepare_thread_safety(new_entry);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(new_entry->section);
        cma_free(new_entry->key);
        cma_free(new_entry->value);
        new_entry->section = ft_nullptr;
        new_entry->key = ft_nullptr;
        new_entry->value = ft_nullptr;
        return (error_code);
    }
    config->entry_count += 1U;
    return (FT_ERR_SUCCESS);
}

config_data *config_parse(const char *filename)
{
    FILE *file;
    config_data *config;
    ft_string line;
    char buffer[512];
    char *current_section;
    ft_size_t chunk_length;
    int32_t error_code;

    if (!filename)
        return (ft_nullptr);
    file = ft_fopen(filename, "r");
    if (!file)
        return (ft_nullptr);
    config = config_data_create();
    if (!config)
    {
        ft_fclose(file);
        return (ft_nullptr);
    }
    error_code = line.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        config_data_free(config);
        ft_fclose(file);
        return (ft_nullptr);
    }
    current_section = ft_nullptr;
    while (ft_fgets(buffer, sizeof(buffer), file))
    {
        chunk_length = ft_strlen(buffer);
        error_code = line.append(buffer, chunk_length);
        if (error_code != FT_ERR_SUCCESS)
            break ;
        if (chunk_length != 0U
            && (buffer[chunk_length - 1U] == '\n'
                || buffer[chunk_length - 1U] == '\r'))
        {
            error_code = config_parse_line(config, line.data(),
                &current_section);
            if (error_code != FT_ERR_SUCCESS)
                break ;
            error_code = line.clear();
            if (error_code != FT_ERR_SUCCESS)
                break ;
        }
    }
    if (error_code == FT_ERR_SUCCESS && std::ferror(file) != 0)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS && line.size() != 0U)
        error_code = config_parse_line(config, line.data(),
            &current_section);
    if (line.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INTERNAL;
    cma_free(current_section);
    ft_fclose(file);
    if (error_code != FT_ERR_SUCCESS)
    {
        config_data_free(config);
        return (ft_nullptr);
    }
    return (config);
}

static config_data *config_parse_json_internal(const char *filename)
{
    if (!filename)
    {
        return (ft_nullptr);
    }
    FILE *file;
    json_group *groups;

    file = ft_fopen(filename, "r");
    if (!file)
        return (ft_nullptr);
    groups = json_read_from_file_stream(file, 512);
    ft_fclose(file);
    if (!groups)
        return (ft_nullptr);
    ft_size_t count;
    count = 0;
    json_group *group_pointer = groups;
    while (group_pointer)
    {
        json_item *item_pointer = group_pointer->items;
        while (item_pointer)
        {
            ++count;
            item_pointer = item_pointer->next;
        }
        group_pointer = group_pointer->next;
    }
    config_data *config = config_data_create();
    if (!config)
    {
        json_free_groups(groups);
        return (ft_nullptr);
    }
    if (count)
    {
        config->entries = static_cast<config_entry*>(adv_calloc(count, sizeof(config_entry)));
        if (!config->entries)
        {
            config_data_teardown_thread_safety(config);
            cma_free(config);
            json_free_groups(groups);
            return (ft_nullptr);
        }
    }
    ft_size_t index;
    index = 0;
    group_pointer = groups;
    while (group_pointer)
    {
        json_item *item_pointer = group_pointer->items;
        while (item_pointer)
        {
            config_entry *entry = &config->entries[index];
            entry->mutex = ft_nullptr;
            if (group_pointer->name)
            {
                entry->section = adv_strdup(group_pointer->name);
                if (!entry->section)
                {
                    config->entry_count = index;
                    config_data_free(config);
                    json_free_groups(groups);
                    return (ft_nullptr);
                }
            }
            if (item_pointer->key)
            {
                entry->key = adv_strdup(item_pointer->key);
                if (!entry->key)
                {
                    config->entry_count = index + 1;
                    config_data_free(config);
                    json_free_groups(groups);
                    return (ft_nullptr);
                }
            }
            if (item_pointer->value)
            {
                entry->value = adv_strdup(item_pointer->value);
                if (!entry->value)
                {
                    config->entry_count = index + 1;
                    config_data_free(config);
                    json_free_groups(groups);
                    return (ft_nullptr);
                }
            }
            if (config_entry_prepare_thread_safety(entry) != FT_ERR_SUCCESS)
            {
                config->entry_count = index + 1;
                config_data_free(config);
                json_free_groups(groups);
                return (ft_nullptr);
            }
            ++index;
            item_pointer = item_pointer->next;
        }
        group_pointer = group_pointer->next;
    }
    config->entry_count = count;
    json_free_groups(groups);
    return (config);
}

config_data *config_load_env()
{
    char **environment_entries;
    config_data *config = config_data_create();
    if (!config)
        return (ft_nullptr);
    ft_size_t count;
    count = 0;
    environment_entries = cmp_get_environ_entries();
    if (environment_entries)
    {
        while (environment_entries[count])
            ++count;
    }
    if (!count)
    {
        return (config);
    }
    config->entries = static_cast<config_entry*>(adv_calloc(count, sizeof(config_entry)));
    if (!config->entries)
    {
        config_data_teardown_thread_safety(config);
        cma_free(config);
        return (ft_nullptr);
    }
    ft_size_t index;
    index = 0;
    while (index < count)
    {
        char *pair = environment_entries[index];
        char *equals_sign = ft_nullptr;
        if (pair)
            equals_sign = ft_strchr(pair, '=');
        config_entry *entry = &config->entries[index];
        entry->mutex = ft_nullptr;
        if (equals_sign)
        {
            ft_size_t key_length = static_cast<ft_size_t>(equals_sign - pair);
            entry->key = static_cast<char*>(adv_calloc(key_length + 1, sizeof(char)));
            if (!entry->key)
            {
                config->entry_count = index;
                config_data_free(config);
                return (ft_nullptr);
            }
            ft_memcpy(entry->key, pair, key_length);
            if (equals_sign[1])
            {
                entry->value = adv_strdup(equals_sign + 1);
                if (!entry->value)
                {
                    config->entry_count = index + 1;
                    config_data_free(config);
                    return (ft_nullptr);
                }
            }
        }
        else if (pair)
        {
            entry->key = adv_strdup(pair);
            if (!entry->key)
            {
                config->entry_count = index;
                config_data_free(config);
                return (ft_nullptr);
            }
        }
        entry->section = ft_nullptr;
        if (config_entry_prepare_thread_safety(entry) != FT_ERR_SUCCESS)
        {
            config->entry_count = index + 1;
            config_data_free(config);
            return (ft_nullptr);
        }
        ++index;
    }
    config->entry_count = count;
    return (config);
}

config_data *config_load_file(const char *filename)
{
    config_data *config;

    if (!filename)
    {
        return (ft_nullptr);
    }
    const char *dot = ft_strrchr(filename, '.');
    if (dot && ft_strcmp(dot, ".json") == 0)
    {
        config = config_parse_json_internal(filename);
        if (config)
            return (config);
    }
    config = config_parse(filename);
    if (config)
        return (config);
    return (ft_nullptr);
}
