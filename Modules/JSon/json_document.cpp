#include "document.hpp"
#include "json_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <new>

#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local int32_t g_json_document_last_error = FT_ERR_SUCCESS;
static thread_local pt_recursive_mutex *g_json_document_locked_mutex = ft_nullptr;

static void json_document_set_error(int32_t error_code)
{
    g_json_document_last_error = error_code;
    return ;
}

static int32_t json_document_last_error(void)
{
    return (g_json_document_last_error);
}

static constexpr ft_size_t JSON_POINTER_INLINE_TOKEN_BYTES = 128U;
using json_pointer_storage = std::unique_ptr<char, decltype(&cma_free)>;

static int32_t json_document_pointer_token(const char *&cursor, std::string_view &token,
    char (&scratch)[JSON_POINTER_INLINE_TOKEN_BYTES], json_pointer_storage &storage) noexcept
{
    if (*cursor != '/')
        return (FT_ERR_INVALID_ARGUMENT);
    const char *start = ++cursor;
    ft_bool escaped = FT_FALSE;
    while (*cursor != '\0' && *cursor != '/')
    {
        if (*cursor == '~')
            escaped = FT_TRUE;
        ++cursor;
    }
    const ft_size_t length = static_cast<ft_size_t>(cursor - start);
    if (length == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!escaped)
    {
        token = std::string_view(start, length);
        return (FT_ERR_SUCCESS);
    }

    char *decoded = scratch;
    if (length > sizeof(scratch))
    {
        storage.reset(static_cast<char *>(cma_malloc(length)));
        decoded = storage.get();
        if (decoded == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    ft_size_t input = 0U;
    ft_size_t output = 0U;
    while (input < length)
    {
        char character = start[input++];
        if (character == '~')
        {
            if (input == length)
                return (FT_ERR_INVALID_ARGUMENT);
            const char escape = start[input++];
            if (escape == '0')
                character = '~';
            else if (escape == '1')
                character = '/';
            else
                return (FT_ERR_INVALID_ARGUMENT);
        }
        decoded[output++] = character;
    }
    token = std::string_view(decoded, output);
    return (FT_ERR_SUCCESS);
}

static ft_bool json_document_pointer_matches(const char *name, std::string_view token) noexcept
{
    return (name != ft_nullptr && std::strncmp(name, token.data(), token.size()) == 0
        && name[token.size()] == '\0');
}

static void json_document_finalize_guard() noexcept
{
    int32_t operation_error = json_document_last_error();

    if (g_json_document_locked_mutex != ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(g_json_document_locked_mutex);
        g_json_document_locked_mutex = ft_nullptr;
    }
    if (operation_error != FT_ERR_SUCCESS)
    {
        json_document_set_error(operation_error);
        return ;
    }
    json_document_set_error(FT_ERR_SUCCESS);
    return ;
}

void json_document::set_error_unlocked(int32_t error_code) const noexcept
{
    json_document_set_error(error_code);
    this->_error_code = error_code;
    return ;
}

void json_document::set_error(int32_t error_code) const noexcept
{
    this->set_error_unlocked(error_code);
    return ;
}

void json_document::clear_unlocked() noexcept
{
    json_free_groups(this->_groups);
    this->_groups = ft_nullptr;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    return ;
}

char *json_document::write_to_string_unlocked(ft_size_t &length) const noexcept
{
    char *result;
    this->set_error_unlocked(json_write_buffer(this->_groups, result, length));
    return (result);
}

int32_t json_document_serialize_buffer(const json_document &document,
    char *&output, ft_size_t &length) noexcept
{
    output = ft_nullptr;
    length = 0U;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(document._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        document.set_error(lock_error);
        return (lock_error);
    }
    output = document.write_to_string_unlocked(length);
    const int32_t error = document._error_code;
    (void)pt_recursive_mutex_unlock_if_not_null(document._mutex);
    return (error);
}

int32_t json_document_replace_groups(json_document &document, json_group *groups) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(document._initialised_state,
        "json_document_replace_groups");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(document._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    json_group *previous = document._groups;
    document._groups = groups;
    document.set_error_unlocked(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(document._mutex);
    if (previous != ft_nullptr)
        json_free_groups(previous);
    return (FT_ERR_SUCCESS);
}

json_document::json_document() noexcept
    : _groups(ft_nullptr)
    , _error_code(FT_ERR_SUCCESS)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error_unlocked(FT_ERR_SUCCESS);
    return ;
}

json_document::~json_document() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t json_document::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "json_document::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

static int32_t json_document_clone_groups(const json_group *source, json_group *&copy) noexcept
{
    json_group **next_group = &copy;
    while (source != ft_nullptr)
    {
        const char *name = source->name;
        if (name == ft_nullptr)
            name = "";
        json_group *group = json_create_json_group(name);
        if (group == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        *next_group = group;
        next_group = &group->next;
        json_item **next_item = &group->items;
        const json_item *source_item = source->items;
        while (source_item != ft_nullptr)
        {
            const char *key = source_item->key;
            const char *value = source_item->value;
            if (key == ft_nullptr)
                key = "";
            if (value == ft_nullptr)
                value = "";
            json_item *item = json_create_item(key, value);
            if (item == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            *next_item = item;
            next_item = &item->next;
            const int32_t error = json_item_get_error(item);
            if (error != FT_ERR_SUCCESS)
                return (error);
            source_item = source_item->next;
        }
        source = source->next;
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_document::initialize(const json_document &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "json_document::initialize(const json_document &) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    json_group *copy = ft_nullptr;
    ft_bool enable_mutex = FT_FALSE;
    const ft_bool source_destroyed = other._initialised_state == FT_CLASS_STATE_DESTROYED;
    if (!source_destroyed)
    {
        const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        enable_mutex = other._mutex != ft_nullptr;
        const int32_t copy_error = json_document_clone_groups(other._groups, copy);
        (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
        if (copy_error != FT_ERR_SUCCESS)
        {
            json_free_groups(copy);
            return (copy_error);
        }
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
        {
            json_free_groups(copy);
            return (operation_error);
        }
    }
    if (source_destroyed)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    int32_t operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
    {
        json_free_groups(copy);
        return (operation_error);
    }
    this->_groups = copy;
    if (enable_mutex)
    {
        operation_error = this->enable_thread_safety();
        if (operation_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (operation_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_document::initialize(json_document &&other) noexcept
{
    int32_t operation_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "json_document::initialize(json_document &&) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
            return (operation_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    if (other._mutex != ft_nullptr)
    {
        operation_error = this->enable_thread_safety();
        if (operation_error != FT_ERR_SUCCESS)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
            (void)this->destroy();
            return (operation_error);
        }
    }
    this->_groups = other._groups;
    this->_error_code = other._error_code;
    other._groups = ft_nullptr;
    other._error_code = FT_ERR_SUCCESS;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t json_document::destroy() noexcept
{
    int32_t lock_error;
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        this->clear_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

uint32_t json_document::move(json_document &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(
            static_cast<json_document &&>(other))));
}

int32_t json_document::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "json_document::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t json_document::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "json_document::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool json_document::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

json_group *json_document::create_group(const char *name) noexcept
{
    int32_t lock_error;
    json_group *group;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!name)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    group = json_create_json_group(name);
    if (!group)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_NO_MEMORY;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (group);
}

json_item *json_document::create_item(const char *key, const char *value) noexcept
{
    int32_t lock_error;
    json_item *item;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!key || !value)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = json_create_item(key, value);
    if (!item)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_NO_MEMORY;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (item);
}

json_item *json_document::create_item(const char *key, const ft_big_number &value) noexcept
{
    int32_t lock_error;
    json_item *item;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!key)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = json_create_item(key, value);
    if (!item)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_NO_MEMORY;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (item);
}

json_item *json_document::create_item(const char *key, const int32_t value) noexcept
{
    int32_t lock_error;
    json_item *item;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!key)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = json_create_item(key, value);
    if (!item)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_NO_MEMORY;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (item);
}

json_item *json_document::create_item(const char *key, const ft_bool value) noexcept
{
    int32_t lock_error;
    json_item *item;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!key)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = json_create_item(key, value);
    if (!item)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_NO_MEMORY;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (item);
}

void json_document::add_item(json_group *group, json_item *item) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    if (!group || !item)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return ;
    }
    json_add_item_to_group(group, item);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return ;
}

void json_document::append_group(json_group *group) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    if (!group)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return ;
    }
    json_append_group(&this->_groups, group);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return ;
}

int32_t json_document::write_to_file(const char *file_path) const noexcept
{
    int32_t lock_error;
    int32_t result;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    if (!file_path)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    result = json_write_to_file(file_path, this->_groups);
    if (result != FT_ERR_SUCCESS)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_INVALID_HANDLE;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (result);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (FT_ERR_SUCCESS);
}

int32_t json_document::write_to_backend(ft_document_sink &sink) const noexcept
{
    char *serialized_content;
    ft_size_t serialized_length;
    const int32_t build_error = json_document_serialize_buffer(*this,
        serialized_content, serialized_length);
    if (build_error != FT_ERR_SUCCESS)
        return (build_error);
    const int32_t write_error = sink.write_all(serialized_content, serialized_length);
    cma_free(serialized_content);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->set_error_unlocked(write_error);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (write_error);
}

char *json_document::write_to_string() const noexcept
{
    char *result;
    ft_size_t length;
    (void)json_document_serialize_buffer(*this, result, length);
    return (result);
}

int32_t json_document::read_from_file(const char *file_path) noexcept
{
    int32_t lock_error;
    json_group *groups;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    if (!file_path)
    {
        this->clear_unlocked();
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->clear_unlocked();
    groups = json_read_from_file(file_path);
    if (!groups)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_INVALID_ARGUMENT;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (current_error);
    }
    this->_groups = groups;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (FT_ERR_SUCCESS);
}

int32_t json_document::read_from_backend(ft_document_source &source) noexcept
{
    int32_t lock_error;
    json_group *groups;
    int32_t error_code;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    this->clear_unlocked();
    groups = json_read_from_backend(source);
    if (!groups)
    {
        error_code = json_document_last_error();
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_INVALID_ARGUMENT;
        this->set_error_unlocked(error_code);
        json_document_finalize_guard();
        return (error_code);
    }
    this->_groups = groups;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (FT_ERR_SUCCESS);
}

int32_t json_document::read_from_file_streaming(const char *file_path, ft_size_t buffer_capacity) noexcept
{
    int32_t lock_error;
    FILE *file;
    json_group *groups;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    if (!file_path || buffer_capacity == FT_ERR_SUCCESS)
    {
        this->clear_unlocked();
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    file = fopen(file_path, "rb");
    if (!file)
    {
        this->clear_unlocked();
        this->set_error_unlocked(FT_ERR_IO);
        json_document_finalize_guard();
        return (FT_ERR_IO);
    }
    this->clear_unlocked();
    groups = json_read_from_file_stream(file, buffer_capacity);
    fclose(file);
    if (!groups)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_INVALID_ARGUMENT;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (current_error);
    }
    this->_groups = groups;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (FT_ERR_SUCCESS);
}

int32_t json_document::read_from_string(const char *content) noexcept
{
    int32_t lock_error;
    json_group *groups;
    int32_t current_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    if (!content)
    {
        this->clear_unlocked();
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->clear_unlocked();
    groups = json_read_from_string(content);
    if (!groups)
    {
        current_error = json_document_last_error();
        if (current_error == FT_ERR_SUCCESS)
            current_error = FT_ERR_INVALID_ARGUMENT;
        this->set_error_unlocked(current_error);
        json_document_finalize_guard();
        return (current_error);
    }
    this->_groups = groups;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (FT_ERR_SUCCESS);
}

json_group *json_document::find_group(const char *name) const noexcept
{
    int32_t lock_error;
    json_group *group;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!name)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    group = json_find_group(this->_groups, name);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (group);
}

json_item *json_document::find_item(json_group *group, const char *key) const noexcept
{
    int32_t lock_error;
    json_item *item;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!group || !key)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = json_find_item(group, key);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (item);
}

json_item *json_document::find_item_by_pointer_unlocked(const char *pointer) const noexcept
{
    if (pointer == ft_nullptr)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    const char *cursor = pointer;
    std::string_view token;
    char scratch[JSON_POINTER_INLINE_TOKEN_BYTES];
    json_pointer_storage storage(nullptr, cma_free);
    int32_t error = json_document_pointer_token(cursor, token, scratch, storage);
    if (error != FT_ERR_SUCCESS)
    {
        this->set_error_unlocked(error);
        return (ft_nullptr);
    }
    json_group *group = this->_groups;
    while (group != ft_nullptr && !json_document_pointer_matches(group->name, token))
        group = group->next;
    if (group == ft_nullptr)
    {
        this->set_error_unlocked(FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    error = json_document_pointer_token(cursor, token, scratch, storage);
    if (error != FT_ERR_SUCCESS)
    {
        this->set_error_unlocked(error);
        return (ft_nullptr);
    }
    json_item *item = group->items;
    while (item != ft_nullptr && !json_document_pointer_matches(item->key, token))
        item = item->next;
    if (item == ft_nullptr)
    {
        this->set_error_unlocked(FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    if (*cursor != '\0')
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    this->set_error_unlocked(FT_ERR_SUCCESS);
    return (item);
}

json_item *json_document::find_item_by_pointer(const char *pointer) const noexcept
{
    int32_t lock_error;
    json_item *item;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = this->find_item_by_pointer_unlocked(pointer);
    json_document_finalize_guard();
    return (item);
}

const char *json_document::get_value_by_pointer(const char *pointer) const noexcept
{
    int32_t lock_error;
    json_item *item;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    item = this->find_item_by_pointer_unlocked(pointer);
    if (!item)
    {
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    if (!item->value)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    const char *value = item->value;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (value);
}

void json_document::remove_group(const char *name) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    if (!name)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return ;
    }
    json_remove_group(&this->_groups, name);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return ;
}

void json_document::remove_item(json_group *group, const char *key) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    if (!group || !key)
    {
        this->set_error_unlocked(FT_ERR_INVALID_ARGUMENT);
        json_document_finalize_guard();
        return ;
    }
    json_remove_item(group, key);
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return ;
}

template <typename Value>
void json_document::update_item_checked(json_group *group, const char *key,
    const Value &value) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    g_json_document_locked_mutex = this->_mutex;
    int32_t error = json_update_item_checked(group, key, value);
    if (error == FT_ERR_NOT_FOUND)
        error = FT_ERR_INVALID_ARGUMENT;
    this->set_error_unlocked(error);
    json_document_finalize_guard();
    return ;
}

void json_document::update_item(json_group *group, const char *key, const char *value) noexcept
{
    this->update_item_checked(group, key, value);
    return ;
}

void json_document::update_item(json_group *group, const char *key, int32_t value) noexcept
{
    this->update_item_checked(group, key, value);
    return ;
}

void json_document::update_item(json_group *group, const char *key, ft_bool value) noexcept
{
    this->update_item_checked(group, key, value);
    return ;
}

void json_document::update_item(json_group *group, const char *key, const ft_big_number &value) noexcept
{
    this->update_item_checked(group, key, value);
    return ;
}

void json_document::clear() noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return ;
    }
    this->clear_unlocked();
    json_document_finalize_guard();
    return ;
}

json_group *json_document::get_groups() const noexcept
{
    int32_t lock_error;
    json_group *groups;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_nullptr);
    }
    groups = this->_groups;
    this->set_error_unlocked(FT_ERR_SUCCESS);
    json_document_finalize_guard();
    return (groups);
}

void json_document::set_manual_error(int32_t error_code) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "json_document::set_manual_error");
    this->set_error(error_code);
    return ;
}

int32_t json_document::get_error() const noexcept
{
    int32_t lock_error;
    int32_t error_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "json_document::get_error");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (lock_error);
    }
    error_value = this->_error_code;
    json_document_finalize_guard();
    return (error_value);
}

const char *json_document::get_error_str() const noexcept
{
    int32_t lock_error;
    const char *error_string;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "json_document::get_error_str");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error == FT_ERR_SUCCESS)
        g_json_document_locked_mutex = this->_mutex;
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        json_document_finalize_guard();
        return (ft_strerror(lock_error));
    }
    error_string = ft_strerror(this->_error_code);
    json_document_finalize_guard();
    return (error_string);
}
