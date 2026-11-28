#include "class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_string::ft_string() noexcept
    : _data(ft_nullptr)
    , _length(0)
    , _capacity(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _last_error(FT_ERR_SUCCESS)
{
    return ;
}

ft_string *ft_string::from_error(int32_t error_code) noexcept
{
    ft_string *value;
    int32_t initialization_error;

    value = new (std::nothrow) ft_string();
    if (value == ft_nullptr)
        return (ft_nullptr);
    initialization_error = value->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete value;
        return (ft_nullptr);
    }
    else
        value->set_error(error_code);
    return (value);
}

int32_t ft_string::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_string::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = ft_nullptr;
    this->_length = 0;
    this->_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_string::initialize(const char *initial_string) noexcept
{
    int32_t initialization_error = this->initialize();
    int32_t assign_error;

    if (initialization_error != FT_ERR_SUCCESS)
        return (this->set_error(initialization_error));
    if (initial_string)
    {
        assign_error = this->assign(initial_string, ft_strlen_size_t(initial_string));
        if (assign_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(assign_error));
        }
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_string::initialize(ft_size_t count, char character) noexcept
{
    int32_t initialization_error = this->initialize();
    int32_t assign_error;

    if (initialization_error != FT_ERR_SUCCESS)
        return (this->set_error(initialization_error));
    assign_error = this->assign(count, character);
    if (assign_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(assign_error));
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_string::initialize(const ft_string &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_string::initialize(const ft_string &) source",
            "called with uninitialised source object");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            int32_t destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                    return (this->set_error(destroy_error));
        }
        this->_data = ft_nullptr;
        this->_length = 0;
        this->_capacity = 0;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(other.get_error()));
    }
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (this->set_error(destroy_error));
    }
    int32_t initialization_error = this->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
        return (this->set_error(initialization_error));
    const int32_t other_lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (other_lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(other_lock_error));
    }
    if (other._data != ft_nullptr)
    {
        this->_data = static_cast<char *>(cma_malloc(other._capacity + 1));
        if (this->_data == ft_nullptr)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
            (void)this->destroy();
            return (this->set_error(FT_ERR_NO_MEMORY));
        }
        (void)ft_memcpy(this->_data, other._data, other._length + 1);
        this->_capacity = other._capacity;
    }
    this->_length = other._length;
    const int32_t source_error = other.get_error();
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (this->set_error(source_error));
}

int32_t ft_string::initialize(ft_string &&other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_string::initialize(ft_string &&) source",
            "called with uninitialised source object");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            int32_t destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                    return (this->set_error(destroy_error));
        }
        this->_data = ft_nullptr;
        this->_length = 0;
        this->_capacity = 0;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(other.get_error()));
    }
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (this->set_error(destroy_error));
    }
    int32_t move_error = this->move(other);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(move_error));
    }
    return (this->set_error(other.get_error()));
}

int32_t ft_string::destroy() noexcept
{
    int32_t thread_safety_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    thread_safety_error = this->disable_thread_safety();
    cma_free(this->_data);
    this->_data = ft_nullptr;
    this->_length = 0;
    this->_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (thread_safety_error != FT_ERR_SUCCESS)
        return (this->set_error(thread_safety_error));
    return (this->set_error(FT_ERR_SUCCESS));
}

ft_string::~ft_string()
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return ;
    }
    (void)this->destroy();
    return ;
}

ft_string &ft_string::operator=(const ft_string &other) noexcept
{
    int32_t assignment_error;

    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_string::operator=(const ft_string &) source",
            "called with uninitialised source object");
        this->set_error(FT_ERR_INVALID_STATE);
        return (*this);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            assignment_error = this->destroy();
            if (assignment_error != FT_ERR_SUCCESS)
            {
                this->set_error(assignment_error);
                return (*this);
            }
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    (void)this->initialize(other);
    return (*this);
}

ft_string &ft_string::operator=(ft_string &&other) noexcept
{
    int32_t assignment_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_string::operator=(ft_string &&) source",
            "called with uninitialised source object");
        this->set_error(FT_ERR_INVALID_STATE);
        return (*this);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            assignment_error = this->destroy();
            if (assignment_error != FT_ERR_SUCCESS)
            {
                this->set_error(assignment_error);
                return (*this);
            }
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    (void)this->initialize(static_cast<ft_string &&>(other));
    return (*this);
}

ft_string &ft_string::operator=(const char *string) noexcept
{
    int32_t assignment_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        if (string == ft_nullptr)
            assignment_error = this->initialize();
        else
            assignment_error = this->initialize(string);
        if (assignment_error != FT_ERR_SUCCESS)
            this->set_error(assignment_error);
        else
            this->set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    if (string == ft_nullptr)
        assignment_error = this->clear();
    else
        assignment_error = this->assign(string, ft_strlen_size_t(string));
    this->set_error(assignment_error);
    return (*this);
}

ft_string &ft_string::operator=(char character) noexcept
{
    char buffer[2];

    buffer[0] = character;
    buffer[1] = '\0';
    return ((*this) = buffer);
}
