#include "dom.hpp"
#include "../CMA/CMA.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../Basic/basic.hpp"
#include "../Printf/printf.hpp"
#include "../Errno/errno_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Template/move.hpp"

ft_dom_node::ft_dom_node() noexcept
    : _type(FT_DOM_NODE_NULL), _name(), _value(), _children(),
      _attribute_keys(), _attribute_values(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_dom_node::~ft_dom_node() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_dom_node::initialize() noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_dom_node::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_type = FT_DOM_NODE_NULL;

    if (this->_children.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        this->_children.clear();
        if (this->_children.get_error() != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->_children.get_error());
        }
    }
    else
    {
        initialize_error = this->_children.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    if (this->_attribute_keys.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        this->_attribute_keys.clear();
        if (this->_attribute_keys.get_error() != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->_attribute_keys.get_error());
        }
    }
    else
    {
        initialize_error = this->_attribute_keys.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    if (this->_attribute_values.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        this->_attribute_values.clear();
        if (this->_attribute_values.get_error() != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (this->_attribute_values.get_error());
        }
    }
    else
    {
        initialize_error = this->_attribute_values.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    if (this->_name.is_initialised() == FT_TRUE)
    {
        initialize_error = this->_name.clear();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    else
    {
        initialize_error = this->_name.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    if (this->_value.is_initialised() == FT_TRUE)
    {
        initialize_error = this->_value.clear();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    else
    {
        initialize_error = this->_value.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (initialize_error);
        }
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_node::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    int32_t status = this->disable_thread_safety();
    ft_size_t child_index = 0;
    const ft_size_t child_count = this->_children.size();
    while (child_index < child_count)
    {
        delete this->_children[child_index];
        child_index += 1;
    }
    const int32_t release_statuses[] = {
        this->_children.destroy(),
        this->_attribute_keys.destroy(),
        this->_attribute_values.destroy(),
        this->_name.destroy(),
        this->_value.destroy()
    };
    ft_size_t status_index = 0;
    while (status == FT_ERR_SUCCESS && status_index < sizeof(release_statuses) / sizeof(release_statuses[0]))
    {
        status = release_statuses[status_index];
        ++status_index;
    }
    this->_type = FT_DOM_NODE_NULL;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (status);
}

int32_t ft_dom_node::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::enable_thread_safety");
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

int32_t ft_dom_node::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_dom_node::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t ft_dom_node::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_node::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == false)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_node::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::lock");
    return (this->lock_internal(lock_acquired));
}

void ft_dom_node::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_dom_node_type ft_dom_node::get_type() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_dom_node_type type;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_type");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_DOM_NODE_NULL);
    type = this->_type;
    (void)this->unlock_internal(lock_acquired);
    return (type);
}

void ft_dom_node::set_type(ft_dom_node_type type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::set_type");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_type = type;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t ft_dom_node::set_name(const ft_string &name) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::set_name(ft_string)");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_name = name;
    const int32_t status = this->_name.get_error();
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

int32_t ft_dom_node::set_name(const char *name) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::set_name(const char *)");
    if (name == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->_name.assign(name, ft_strlen_size_t(name));
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

const ft_string &ft_dom_node::get_name() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_name");
    return (this->_name);
}

int32_t ft_dom_node::set_value(const ft_string &value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::set_value(ft_string)");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_value = value;
    const int32_t status = this->_value.get_error();
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

int32_t ft_dom_node::set_value(const char *value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::set_value(const char *)");
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->_value.assign(value, ft_strlen_size_t(value));
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

const ft_string &ft_dom_node::get_value() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_value");
    return (this->_value);
}

int32_t ft_dom_node::add_child(ft_dom_node *child) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::add_child");
    if (child == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->_children.push_back(child);
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

const ft_vector<ft_dom_node*> &ft_dom_node::get_children() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_children");
    return (this->_children);
}

template <typename Text>
static ft_size_t dom_find_attribute_index(const ft_vector<ft_string> &keys, const Text &key) noexcept
{
    const ft_size_t count = keys.size();
    ft_size_t index = 0;
    while (index < count)
    {
        if (keys[index] == key)
            return (index);
        ++index;
    }
    return (FT_SYSTEM_SIZE_MAX);
}

static int32_t dom_assign_attribute(ft_string &output, const ft_string &value) noexcept
{
    output = value;
    return (output.get_error());
}

static int32_t dom_assign_attribute(ft_string &output, const char *value) noexcept
{
    return (output.assign(value, ft_strlen_size_t(value)));
}

template <typename Text>
int32_t ft_dom_node::add_attribute_unlocked(const Text &key, const Text &value) noexcept
{
    const ft_size_t index = dom_find_attribute_index(this->_attribute_keys, key);
    if (index != FT_SYSTEM_SIZE_MAX)
        return (dom_assign_attribute(this->_attribute_values[index], value));

    ft_string owned_key;
    ft_string owned_value;
    int32_t status = owned_key.initialize(key);
    if (status == FT_ERR_SUCCESS)
        status = owned_value.initialize(value);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = this->_attribute_keys.push_back(ft_move(owned_key));
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = this->_attribute_values.push_back(ft_move(owned_value));
    if (status != FT_ERR_SUCCESS)
        this->_attribute_keys.pop_back();
    return (status);
}

int32_t ft_dom_node::add_attribute(const ft_string &key, const ft_string &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::add_attribute(ft_string,ft_string)");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->add_attribute_unlocked(key, value);
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

int32_t ft_dom_node::add_attribute(const char *key, const char *value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::add_attribute(const char *,const char *)");
    if (key == ft_nullptr || value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = this->add_attribute_unlocked(key, value);
    (void)this->unlock_internal(lock_acquired);
    return (status);
}

ft_bool ft_dom_node::has_attribute(const ft_string &key) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::has_attribute");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_bool found = dom_find_attribute_index(this->_attribute_keys, key) != FT_SYSTEM_SIZE_MAX;
    (void)this->unlock_internal(lock_acquired);
    return (found);
}

ft_string *ft_dom_node::get_attribute(const ft_string &key) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_attribute");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (ft_nullptr);

    ft_string *value = new (std::nothrow) ft_string();
    int32_t status = FT_ERR_NO_MEMORY;
    if (value)
    {
        const ft_size_t index = dom_find_attribute_index(this->_attribute_keys, key);
        status = value->initialize();
        if (status == FT_ERR_SUCCESS && index != FT_SYSTEM_SIZE_MAX)
            status = dom_assign_attribute(*value, this->_attribute_values[index]);
    }
    (void)this->unlock_internal(lock_acquired);
    if (status != FT_ERR_SUCCESS)
    {
        delete value;
        return (ft_nullptr);
    }
    return (value);
}

const ft_vector<ft_string> &ft_dom_node::get_attribute_keys() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_attribute_keys");
    return (this->_attribute_keys);
}

const ft_vector<ft_string> &ft_dom_node::get_attribute_values() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::get_attribute_values");
    return (this->_attribute_values);
}

template <typename Matches>
ft_dom_node *ft_dom_node::find_child_if(const Matches &matches) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_node::find_child");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    const ft_size_t count = this->_children.size();
    ft_size_t index = 0;
    ft_dom_node *found = ft_nullptr;
    while (index < count)
    {
        ft_dom_node *child = this->_children[index];
        if (child && matches(child->get_name()))
        {
            found = child;
            break ;
        }
        ++index;
    }
    (void)this->unlock_internal(lock_acquired);
    return (found);
}

ft_dom_node *ft_dom_node::find_child(const ft_string &name) const noexcept
{
    return (this->find_child_if([&name](const ft_string &candidate)
    {
        return (candidate == name);
    }));
}

ft_dom_node *ft_dom_node::find_child(const char *name, ft_size_t length) const noexcept
{
    return (this->find_child_if([name, length](const ft_string &candidate) -> ft_bool
    {
        return (name && candidate.size() == length && ft_memcmp(candidate.c_str(), name, length) == 0);
    }));
}
