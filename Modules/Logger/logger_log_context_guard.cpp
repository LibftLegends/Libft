#include "logger_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/move.hpp"
#include <new>

ft_log_context_guard::ft_log_context_guard() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _group(ft_nullptr),
      _error_code(FT_ERR_SUCCESS)
{
    return ;
}

ft_log_context_guard::~ft_log_context_guard() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_log_context_guard::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_log_context_guard::initialize", "already initialised");
    this->_group = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_context_guard::initialize(const ft_log_context_guard &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_log_context_guard::initialize(copy)", "source object is uninitialised");
    if (this == &other)
        return (FT_ERR_SUCCESS);
    logger_context_retain_group(other._group);
    (void)this->destroy();
    this->_group = other._group;
    this->_initialised_state = other._initialised_state;
    this->set_error(other._error_code);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_context_guard::initialize(ft_log_context_guard &&other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_log_context_guard::initialize(move)", "source object is uninitialised");
    if (this == &other)
        return (FT_ERR_SUCCESS);
    (void)this->destroy();
    this->_group = other._group;
    this->_initialised_state = other._initialised_state;
    this->set_error(other._error_code);
    other._group = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_context_guard::initialize(const s_log_field *fields,
    ft_size_t field_count) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_ERR_NOT_INITIALISED);
    }
    s_log_context_group *group = ft_nullptr;
    if (field_count != 0U)
    {
        if (fields == ft_nullptr)
        {
            this->set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        group = new (std::nothrow) s_log_context_group();
        if (group == ft_nullptr)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            return (FT_ERR_NO_MEMORY);
        }
        const int32_t error = logger_context_push(fields, field_count, group);
        if (error != FT_ERR_SUCCESS)
        {
            logger_context_drop_group(group, FT_FALSE);
            this->set_error(error);
            return (error);
        }
    }
    logger_context_drop_group(this->_group, FT_FALSE);
    this->_group = group;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_context_guard::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    logger_context_drop_group(this->_group, FT_FALSE);
    this->_group = ft_nullptr;
    this->set_error(FT_ERR_SUCCESS);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t ft_log_context_guard::move(ft_log_context_guard &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(ft_move(other))));
}

void ft_log_context_guard::set_error(int32_t error_code_value) const
{
    this->_error_code = error_code_value;
    return ;
}

void ft_log_context_guard::release() noexcept
{
    logger_context_drop_group(this->_group, FT_TRUE);
    this->_group = ft_nullptr;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bool ft_log_context_guard::is_active() const noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_group != ft_nullptr
        && this->_group->released.load(std::memory_order_acquire) == FT_FALSE
        && this->_group->entries.load(std::memory_order_acquire) != 0U);
}

int32_t ft_log_context_guard::get_error() const
{
    return (this->_error_code);
}

const char *ft_log_context_guard::get_error_str() const
{
    return (ft_strerror(this->_error_code));
}
