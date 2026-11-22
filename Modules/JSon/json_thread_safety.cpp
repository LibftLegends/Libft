#include "json.hpp"

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"

static thread_local int32_t g_json_thread_error = FT_ERR_SUCCESS;
static pt_recursive_mutex *g_json_group_list_mutex = ft_nullptr;

static void json_thread_set_error(int32_t error_code)
{
    g_json_thread_error = error_code;
    return ;
}

int32_t json_group_list_lock()
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_json_group_list_mutex);
    json_thread_set_error(lock_error);
    return (lock_error);
}

int32_t json_group_list_lock_manual()
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_json_group_list_mutex);
    json_thread_set_error(lock_error);
    return (lock_error);
}

int32_t json_group_list_unlock_manual()
{
    (void)pt_recursive_mutex_unlock_if_not_null(g_json_group_list_mutex);
    json_thread_set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void json_group_list_finalize_lock()
{
    (void)pt_recursive_mutex_unlock_if_not_null(g_json_group_list_mutex);
    json_thread_set_error(FT_ERR_SUCCESS);
    return ;
}

void json_group_set_error_unlocked(json_group *group, int32_t error_code)
{
    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    group->_error_code = error_code;
    json_thread_set_error(error_code);
    return ;
}

void json_group_set_error(json_group *group, int32_t error_code)
{
    int32_t lock_error;

    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    if (group->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_thread_set_error(lock_error);
        return ;
    }
    json_group_set_error_unlocked(group, error_code);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    json_thread_set_error(error_code);
    return ;
}

int32_t json_group_enable_thread_safety(json_group *group)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (group->_mutex == ft_nullptr)
    {
        mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
        {
            json_thread_set_error(FT_ERR_NO_MEMORY);
            return (FT_ERR_NO_MEMORY);
        }
        initialize_error = mutex_pointer->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            json_thread_set_error(initialize_error);
            return (initialize_error);
        }
        group->_mutex = mutex_pointer;
    }
    json_group_set_error_unlocked(group, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t json_group_disable_thread_safety(json_group *group)
{
    int32_t destroy_error;

    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (group->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = group->_mutex->destroy();
    delete group->_mutex;
    group->_mutex = ft_nullptr;
    json_thread_set_error(destroy_error);
    return (destroy_error);
}

ft_bool json_group_is_thread_safe(const json_group *group)
{
    if (group == ft_nullptr)
        return (FT_FALSE);
    return (group->_mutex != ft_nullptr);
}

int32_t json_group_lock(json_group *group)
{
    int32_t lock_error;

    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (group->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    json_thread_set_error(lock_error);
    return (lock_error);
}

int32_t json_group_get_error(const json_group *group)
{
    if (group == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    json_thread_set_error(FT_ERR_SUCCESS);
    return (group->_error_code);
}

const char *json_group_get_error_str(const json_group *group)
{
    return (ft_strerror(json_group_get_error(group)));
}

void json_item_set_error_unlocked(json_item *item, int32_t error_code)
{
    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    item->_error_code = error_code;
    json_thread_set_error(error_code);
    return ;
}

void json_item_set_error(json_item *item, int32_t error_code)
{
    int32_t lock_error;

    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    if (item->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(item->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_thread_set_error(lock_error);
        return ;
    }
    json_item_set_error_unlocked(item, error_code);
    (void)pt_recursive_mutex_unlock_if_not_null(item->_mutex);
    json_thread_set_error(error_code);
    return ;
}

int32_t json_item_enable_thread_safety(json_item *item)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (item->_mutex == ft_nullptr)
    {
        mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
        {
            json_thread_set_error(FT_ERR_NO_MEMORY);
            return (FT_ERR_NO_MEMORY);
        }
        initialize_error = mutex_pointer->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            json_thread_set_error(initialize_error);
            return (initialize_error);
        }
        item->_mutex = mutex_pointer;
    }
    json_item_set_error_unlocked(item, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t json_item_disable_thread_safety(json_item *item)
{
    int32_t destroy_error;

    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (item->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = item->_mutex->destroy();
    delete item->_mutex;
    item->_mutex = ft_nullptr;
    json_thread_set_error(destroy_error);
    return (destroy_error);
}

ft_bool json_item_is_thread_safe(const json_item *item)
{
    if (item == ft_nullptr)
        return (FT_FALSE);
    return (item->_mutex != ft_nullptr);
}

int32_t json_item_lock(json_item *item)
{
    int32_t lock_error;

    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (item->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(item->_mutex);
    json_thread_set_error(lock_error);
    return (lock_error);
}

int32_t json_item_get_error(const json_item *item)
{
    if (item == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    json_thread_set_error(FT_ERR_SUCCESS);
    return (item->_error_code);
}

const char *json_item_get_error_str(const json_item *item)
{
    return (ft_strerror(json_item_get_error(item)));
}

void json_schema_field_set_error_unlocked(json_schema_field *field, int32_t error_code)
{
    if (field == ft_nullptr)
        return ;
    field->_error_code = error_code;
    return ;
}

void json_schema_field_set_error(json_schema_field *field, int32_t error_code)
{
    int32_t lock_error;

    if (field == ft_nullptr)
        return ;
    if (field->_mutex == ft_nullptr)
        return ;
    lock_error = pt_recursive_mutex_lock_if_not_null(field->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    json_schema_field_set_error_unlocked(field, error_code);
    (void)pt_recursive_mutex_unlock_if_not_null(field->_mutex);
    return ;
}

int32_t json_schema_field_enable_thread_safety(json_schema_field *field)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (field == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (field->_mutex == ft_nullptr)
    {
        mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        initialize_error = mutex_pointer->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            return (initialize_error);
        }
        field->_mutex = mutex_pointer;
    }
    json_schema_field_set_error_unlocked(field, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t json_schema_field_disable_thread_safety(json_schema_field *field)
{
    int32_t destroy_error;

    if (field == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (field->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = field->_mutex->destroy();
    delete field->_mutex;
    field->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool json_schema_field_is_thread_safe(const json_schema_field *field)
{
    if (field == ft_nullptr)
        return (FT_FALSE);
    return (field->_mutex != ft_nullptr);
}

int32_t json_schema_field_lock(json_schema_field *field)
{
    int32_t lock_error;

    if (field == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (field->_mutex == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    lock_error = pt_recursive_mutex_lock_if_not_null(field->_mutex);
    return (lock_error);
}

int32_t json_schema_field_get_error(const json_schema_field *field)
{
    if (field == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (field->_error_code);
}

const char *json_schema_field_get_error_str(const json_schema_field *field)
{
    return (ft_strerror(json_schema_field_get_error(field)));
}

void json_schema_set_error_unlocked(json_schema *schema, int32_t error_code)
{
    if (schema == ft_nullptr)
        return ;
    schema->_error_code = error_code;
    return ;
}

void json_schema_set_error(json_schema *schema, int32_t error_code)
{
    int32_t lock_error;

    if (schema == ft_nullptr)
        return ;
    if (schema->_mutex == ft_nullptr)
        return ;
    lock_error = pt_recursive_mutex_lock_if_not_null(schema->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    json_schema_set_error_unlocked(schema, error_code);
    (void)pt_recursive_mutex_unlock_if_not_null(schema->_mutex);
    return ;
}

int32_t json_schema_enable_thread_safety(json_schema *schema)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (schema == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (schema->_mutex == ft_nullptr)
    {
        mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        initialize_error = mutex_pointer->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            return (initialize_error);
        }
        schema->_mutex = mutex_pointer;
    }
    json_schema_set_error_unlocked(schema, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t json_schema_disable_thread_safety(json_schema *schema)
{
    int32_t destroy_error;

    if (schema == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (schema->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = schema->_mutex->destroy();
    delete schema->_mutex;
    schema->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool json_schema_is_thread_safe(const json_schema *schema)
{
    if (schema == ft_nullptr)
        return (FT_FALSE);
    return (schema->_mutex != ft_nullptr);
}

int32_t json_schema_lock(json_schema *schema)
{
    int32_t lock_error;

    if (schema == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (schema->_mutex == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    lock_error = pt_recursive_mutex_lock_if_not_null(schema->_mutex);
    return (lock_error);
}

int32_t json_schema_get_error(const json_schema *schema)
{
    if (schema == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (schema->_error_code);
}

const char *json_schema_get_error_str(const json_schema *schema)
{
    return (ft_strerror(json_schema_get_error(schema)));
}

void json_stream_reader_set_error_unlocked(json_stream_reader *reader, int32_t error_code)
{
    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    reader->_error_code = error_code;
    json_thread_set_error(error_code);
    return ;
}

void json_stream_reader_set_error(json_stream_reader *reader, int32_t error_code)
{
    int32_t lock_error;

    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    if (reader->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_thread_set_error(lock_error);
        return ;
    }
    json_stream_reader_set_error_unlocked(reader, error_code);
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    json_thread_set_error(error_code);
    return ;
}

int32_t json_stream_reader_enable_thread_safety(json_stream_reader *reader)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (reader->_mutex == ft_nullptr)
    {
        mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
        {
            json_thread_set_error(FT_ERR_NO_MEMORY);
            return (FT_ERR_NO_MEMORY);
        }
        initialize_error = mutex_pointer->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            json_thread_set_error(initialize_error);
            return (initialize_error);
        }
        reader->_mutex = mutex_pointer;
    }
    json_stream_reader_set_error_unlocked(reader, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t json_stream_reader_disable_thread_safety(json_stream_reader *reader)
{
    int32_t destroy_error;

    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (reader->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = reader->_mutex->destroy();
    delete reader->_mutex;
    reader->_mutex = ft_nullptr;
    json_thread_set_error(destroy_error);
    return (destroy_error);
}

ft_bool json_stream_reader_is_thread_safe(const json_stream_reader *reader)
{
    if (reader == ft_nullptr)
        return (FT_FALSE);
    return (reader->_mutex != ft_nullptr);
}

int32_t json_stream_reader_lock(json_stream_reader *reader)
{
    int32_t lock_error;

    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (reader->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    json_thread_set_error(lock_error);
    return (lock_error);
}

void json_stream_reader_finalize_lock(json_stream_reader *reader)
{
    int32_t current_error;

    current_error = g_json_thread_error;
    if (reader == ft_nullptr || reader->_mutex == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    json_stream_reader_set_error_unlocked(reader, current_error);
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    return ;
}

int32_t json_stream_reader_get_error(const json_stream_reader *reader)
{
    if (reader == ft_nullptr)
    {
        json_thread_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    json_thread_set_error(FT_ERR_SUCCESS);
    return (reader->_error_code);
}

const char *json_stream_reader_get_error_str(const json_stream_reader *reader)
{
    return (ft_strerror(json_stream_reader_get_error(reader)));
}
int32_t json_group_list_enable_thread_safety()
{
    if (g_json_group_list_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex = new (std::nothrow) pt_recursive_mutex();
    if (mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t initialize_error = mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex;
        return (initialize_error);
    }
    g_json_group_list_mutex = mutex;
    return (FT_ERR_SUCCESS);
}

int32_t json_group_list_disable_thread_safety()
{
    if (g_json_group_list_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int32_t destroy_error = g_json_group_list_mutex->destroy();
    delete g_json_group_list_mutex;
    g_json_group_list_mutex = ft_nullptr;
    return (destroy_error);
}
