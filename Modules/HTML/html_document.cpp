#include "html_document.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/vector.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "html_parser.hpp"

thread_local int32_t html_document::_last_error = FT_ERR_SUCCESS;

int32_t html_document::set_error(int32_t error_code) noexcept
{
    html_document::_last_error = error_code;
    return (error_code);
}

struct html_clone_frame
{
    const html_node *source_sibling;
    html_node **destination_link;
};

static int32_t html_clone_node_list(const html_node *source_node, html_node **destination_link) noexcept
{
    if (source_node == ft_nullptr)
        return (FT_ERR_SUCCESS);
    ft_vector<html_clone_frame> deferred_siblings;
    int32_t status = deferred_siblings.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);
    while (source_node)
    {
        html_node *new_node = html_create_node(source_node->tag, source_node->text);
        if (new_node == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        *destination_link = new_node;

        const html_attr *source_attribute = source_node->attributes;
        html_attr **attribute_link = &new_node->attributes;
        while (source_attribute)
        {
            html_attr *new_attribute = html_create_attr(source_attribute->key, source_attribute->value);
            if (new_attribute == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            *attribute_link = new_attribute;
            attribute_link = &new_attribute->next;
            source_attribute = source_attribute->next;
        }

        if (source_node->children)
        {
            if (source_node->next)
            {
                status = deferred_siblings.push_back({source_node->next, &new_node->next});
                if (status != FT_ERR_SUCCESS)
                    return (status);
            }
            destination_link = &new_node->children;
            source_node = source_node->children;
            continue ;
        }
        destination_link = &new_node->next;
        source_node = source_node->next;
        if (!source_node && !deferred_siblings.empty())
        {
            const html_clone_frame frame = deferred_siblings[deferred_siblings.size() - 1];
            deferred_siblings.pop_back();
            destination_link = frame.destination_link;
            source_node = frame.source_sibling;
        }
    }
    return (FT_ERR_SUCCESS);
}

static int32_t html_document_lock_two(const pt_recursive_mutex *first_mutex,
    const pt_recursive_mutex *second_mutex) noexcept
{
    int32_t lock_error;

    if (first_mutex == second_mutex)
        return (pt_recursive_mutex_lock_if_not_null(first_mutex));
    if (first_mutex < second_mutex)
    {
        lock_error = pt_recursive_mutex_lock_if_not_null(first_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = pt_recursive_mutex_lock_if_not_null(second_mutex);
        if (lock_error != FT_ERR_SUCCESS)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
            return (lock_error);
        }
    }
    else
    {
        lock_error = pt_recursive_mutex_lock_if_not_null(second_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = pt_recursive_mutex_lock_if_not_null(first_mutex);
        if (lock_error != FT_ERR_SUCCESS)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(second_mutex);
            return (lock_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

static void html_document_unlock_two(const pt_recursive_mutex *first_mutex,
    const pt_recursive_mutex *second_mutex) noexcept
{
    if (first_mutex == second_mutex)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
        return ;
    }
    if (first_mutex < second_mutex)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(second_mutex);
        (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
    }
    else
    {
        (void)pt_recursive_mutex_unlock_if_not_null(first_mutex);
        (void)pt_recursive_mutex_unlock_if_not_null(second_mutex);
    }
    return ;
}

html_document::html_document() noexcept
    : _root(ft_nullptr), _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

html_document::~html_document() noexcept
{
    (void)this->destroy();
    this->_root = ft_nullptr;
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

int32_t html_document::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "html_document::initialize", "already initialised");
    this->_root = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)html_document::set_error(static_cast<uint32_t>(FT_ERR_SUCCESS));
    return (FT_ERR_SUCCESS);
}

int32_t html_document::initialize(const html_document &other) noexcept
{
    int32_t destroy_error;
    int32_t lock_error;
    html_node *cloned_root;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "html_document::initialize(copy)", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_root = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    lock_error = html_document_lock_two(this->_mutex, other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    cloned_root = ft_nullptr;
    const int32_t clone_error = html_clone_node_list(other._root, &cloned_root);
    html_document_unlock_two(this->_mutex, other._mutex);
    if (clone_error != FT_ERR_SUCCESS)
    {
        html_free_nodes(cloned_root);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (clone_error);
    }
    this->_root = cloned_root;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t html_document::initialize(html_document &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "html_document::initialize(move)", "source is uninitialised");
    return (this->move(other));
}

int32_t html_document::move(html_document &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "html_document::move", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_root = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    const int32_t lock_error = html_document_lock_two(this->_mutex, other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INTERNAL);
    }
    this->_root = other._root;
    other._root = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    html_document_unlock_two(this->_mutex, other._mutex);
    return (FT_ERR_SUCCESS);
}

int32_t html_document::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    html_free_nodes(this->_root);
    this->_root = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int32_t html_document::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised(this->_initialised_state, "html_document::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t html_document::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool html_document::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

html_node *html_document::create_node(const char *tag_name,
    const char *text_content) noexcept
{
    int32_t lock_error;
    html_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::create_node");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (tag_name == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    node = html_create_node(tag_name, text_content);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node);
}

html_attr *html_document::create_attr(const char *key, const char *value) noexcept
{
    int32_t lock_error;
    html_attr *attribute;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::create_attr");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (key == ft_nullptr || value == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    attribute = html_create_attr(key, value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (attribute);
}

void html_document::add_attr(html_node *target_node, html_attr *new_attribute) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::add_attr");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (target_node == ft_nullptr || new_attribute == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_add_attr(target_node, new_attribute);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void html_document::remove_attr(html_node *target_node, const char *key) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::remove_attr");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (target_node == ft_nullptr || key == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_remove_attr(target_node, key);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void html_document::add_child(html_node *parent_node, html_node *child_node) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::add_child");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (parent_node == ft_nullptr || child_node == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_add_child(parent_node, child_node);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void html_document::append_node(html_node *new_node) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::append_node");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (new_node == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_append_node(&this->_root, new_node);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t html_document::write_to_file(const char *file_path) const noexcept
{
    int32_t lock_error;
    int32_t write_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::write_to_file");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (file_path == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    write_error = html_write_to_file(file_path, this->_root);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (write_error);
}

char *html_document::write_to_string() const noexcept
{
    int32_t lock_error;
    char *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::write_to_string");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    result = html_write_to_string(this->_root);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

void html_document::remove_nodes_by_tag(const char *tag_name) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::remove_nodes_by_tag");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (tag_name == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_remove_nodes_by_tag(&this->_root, tag_name);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void html_document::remove_nodes_by_attr(const char *key, const char *value) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::remove_nodes_by_attr");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (key == ft_nullptr || value == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_remove_nodes_by_attr(&this->_root, key, value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void html_document::remove_nodes_by_text(const char *text_content) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::remove_nodes_by_text");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (text_content == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    html_remove_nodes_by_text(&this->_root, text_content);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

html_node *html_document::find_by_tag(const char *tag_name) const noexcept
{
    int32_t lock_error;
    html_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::find_by_tag");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (tag_name == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    node = html_find_by_tag(this->_root, tag_name);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node);
}

html_node *html_document::find_by_attr(const char *key, const char *value) const noexcept
{
    int32_t lock_error;
    html_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::find_by_attr");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (key == ft_nullptr || value == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    node = html_find_by_attr(this->_root, key, value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node);
}

html_node *html_document::find_by_text(const char *text_content) const noexcept
{
    int32_t lock_error;
    html_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::find_by_text");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (text_content == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    node = html_find_by_text(this->_root, text_content);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node);
}

html_node *html_document::find_by_selector(const char *selector) const noexcept
{
    int32_t lock_error;
    html_node *node;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::find_by_selector");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (selector == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    node = html_find_by_selector(this->_root, selector);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node);
}

ft_size_t html_document::count_nodes_by_tag(const char *tag_name) const noexcept
{
    int32_t lock_error;
    ft_size_t node_count;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::count_nodes_by_tag");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (static_cast<ft_size_t>(0U));
    if (tag_name == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (static_cast<ft_size_t>(0U));
    }
    node_count = html_count_nodes_by_tag(this->_root, tag_name);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (node_count);
}

html_node *html_document::get_root() const noexcept
{
    int32_t lock_error;
    html_node *root_node;

    errno_abort_if_uninitialised(this->_initialised_state, "html_document::get_root");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    root_node = this->_root;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (root_node);
}

int32_t html_document::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "html_document::get_error");
    return (html_document::_last_error);
}

const char *html_document::get_error_str() const noexcept
{
    const char *message;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "html_document::get_error_str");
    message = ft_strerror(html_document::_last_error);
    if (message == ft_nullptr)
        message = "unknown error";
    return (message);
}

void html_document::clear() noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "html_document::clear");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    html_node *root = this->_root;
    this->_root = ft_nullptr;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    html_free_nodes(root);
    return ;
}
