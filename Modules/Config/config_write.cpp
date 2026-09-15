#include "config.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Printf/printf.hpp"
#include "../File/file_utils.hpp"
#include "../JSon/json.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t config_lock_if_enabled(const config_data *config,
    ft_bool *lock_acquired)
{
    int32_t lock_result;

    if (!config)
        return (FT_ERR_SUCCESS);
    lock_result = pt_mutex_lock_if_not_null(config->mutex);
    if (lock_result == FT_ERR_SUCCESS && lock_acquired)
    {
        if (config->mutex != ft_nullptr)
            *lock_acquired = FT_TRUE;
    }
    return (lock_result);
}

static void config_unlock_guard(const config_data *config, ft_bool lock_acquired)
{
    if (!config || lock_acquired == FT_FALSE)
        return ;
    (void)pt_mutex_unlock_if_not_null(config->mutex);
    return ;
}

static ft_bool config_ini_text_is_representable(const char *text,
    ft_bool section_name, ft_bool key_name)
{
    int32_t length;
    int32_t index;
    unsigned char character;

    if (!text)
        return (FT_TRUE);
    length = ft_strlen(text);
    if (length <= 0)
        return (FT_FALSE);
    character = static_cast<unsigned char>(text[0]);
    if (ft_isspace(static_cast<int32_t>(character)) == FT_TRUE)
        return (FT_FALSE);
    character = static_cast<unsigned char>(text[length - 1]);
    if (ft_isspace(static_cast<int32_t>(character)) == FT_TRUE)
        return (FT_FALSE);
    index = 0;
    while (index < length)
    {
        character = static_cast<unsigned char>(text[index]);
        if (character == '\r' || character == '\n')
            return (FT_FALSE);
        if (section_name == FT_TRUE && character == ']')
            return (FT_FALSE);
        if (key_name == FT_TRUE && character == '=')
            return (FT_FALSE);
        if (key_name == FT_TRUE && index == 0
            && (character == ';' || character == '#'))
            return (FT_FALSE);
        index += 1;
    }
    return (FT_TRUE);
}

static ft_bool config_ini_entry_is_representable(
    const config_entry &entry)
{
    if (config_ini_text_is_representable(entry.section, FT_TRUE, FT_FALSE)
        == FT_FALSE)
        return (FT_FALSE);
    if (config_ini_text_is_representable(entry.key, FT_FALSE, FT_TRUE)
        == FT_FALSE)
        return (FT_FALSE);
    return (config_ini_text_is_representable(entry.value, FT_FALSE,
        FT_FALSE));
}

static int32_t config_write_ini(const config_data *config, const char *filename)
{
    ft_string output;
    const char *last_section;
    ft_size_t entry_index;
    int32_t error_code;

    entry_index = 0;
    while (config && entry_index < config->entry_count)
    {
        if (config_ini_entry_is_representable(config->entries[entry_index])
            == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        entry_index += 1;
    }
    error_code = output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    last_section = ft_nullptr;
    entry_index = 0;
    while (config && entry_index < config->entry_count)
    {
        const config_entry *entry = &config->entries[entry_index];
        if (entry->section)
        {
            if (!last_section || ft_strcmp(entry->section, last_section) != 0)
            {
                error_code = output.append("[");
                if (error_code == FT_ERR_SUCCESS)
                    error_code = output.append(entry->section);
                if (error_code == FT_ERR_SUCCESS)
                    error_code = output.append("]\n");
            }
            last_section = entry->section;
        }
        else
        {
            if (last_section)
            {
                error_code = output.append("[]\n");
            }
            last_section = ft_nullptr;
        }
        if (error_code == FT_ERR_SUCCESS && entry->key && entry->value)
        {
            error_code = output.append(entry->key);
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append("=");
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append(entry->value);
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append("\n");
        }
        else if (error_code == FT_ERR_SUCCESS && entry->key)
        {
            error_code = output.append(entry->key);
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append("=\n");
        }
        else if (error_code == FT_ERR_SUCCESS && entry->value)
        {
            error_code = output.append("=");
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append(entry->value);
            if (error_code == FT_ERR_SUCCESS)
                error_code = output.append("\n");
        }
        if (error_code != FT_ERR_SUCCESS)
        {
            int32_t destroy_error = output.destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
            return (error_code);
        }
        ++entry_index;
    }
    error_code = file_replace_safe(filename, output.c_str(), output.size());
    if (output.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_INTERNAL;
    return (error_code);
}

static json_group *config_find_or_create_group(json_group **groups_head,
    json_group **groups_tail, const char *section_name)
{
    const char *name;
    json_group *current;

    if (!groups_head || !groups_tail)
    {
        return (ft_nullptr);
    }
    name = section_name;
    if (!name)
        name = "";
    current = *groups_head;
    while (current)
    {
        if (current->name && ft_strcmp(current->name, name) == 0)
            return (current);
        current = current->next;
    }
    json_group *new_group = json_create_json_group(name);
    if (!new_group)
        return (ft_nullptr);
    if (!(*groups_head))
        *groups_head = new_group;
    else
        (*groups_tail)->next = new_group;
    *groups_tail = new_group;
    return (new_group);
}

static int32_t config_write_json(const config_data *config, const char *filename)
{
    json_group *groups;
    json_group *groups_tail;
    char *serialized_content;
    int32_t write_result;
    ft_size_t entry_index;

    groups = ft_nullptr;
    groups_tail = ft_nullptr;
    entry_index = 0;
    while (config && entry_index < config->entry_count)
    {
        const config_entry *entry = &config->entries[entry_index];
        if (!entry->key || !entry->value)
        {
            json_free_groups(groups);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        json_group *group = config_find_or_create_group(&groups, &groups_tail,
            entry->section);
        if (!group)
        {
            json_free_groups(groups);
            return (FT_ERR_NO_MEMORY);
        }
        json_item *item = json_create_item(entry->key, entry->value);
        if (!item)
        {
            json_free_groups(groups);
            return (FT_ERR_NO_MEMORY);
        }
        json_add_item_to_group(group, item);
        ++entry_index;
    }
    serialized_content = json_write_to_string(groups);
    if (!serialized_content)
    {
        json_free_groups(groups);
        return (FT_ERR_NO_MEMORY);
    }
    write_result = file_replace_safe(filename, serialized_content,
        ft_strlen(serialized_content));
    cma_free(serialized_content);
    json_free_groups(groups);
    return (write_result);
}

int32_t config_write_file(const config_data *config, const char *filename)
{
    const char *extension;
    ft_bool mutex_locked;
    int32_t lock_error;

    mutex_locked = FT_FALSE;
    if (!config || !filename)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    lock_error = config_lock_if_enabled(config, &mutex_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    if (config->entry_count && !config->entries)
    {
        config_unlock_guard(config, mutex_locked);
        return (FT_ERR_INVALID_STATE);
    }
    extension = ft_strrchr(filename, '.');
    if (extension && ft_strcmp(extension, ".json") == 0)
    {
        int32_t write_result;

        write_result = config_write_json(config, filename);
        config_unlock_guard(config, mutex_locked);
        return (write_result);
    }
    int32_t write_result;

    write_result = config_write_ini(config, filename);
    config_unlock_guard(config, mutex_locked);
    return (write_result);
}
