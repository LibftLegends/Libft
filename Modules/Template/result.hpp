#ifndef FT_RESULT_HPP
# define FT_RESULT_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cstdint>
#include <new>

struct ft_status
{
    int32_t error_code;

    static constexpr ft_status success() noexcept;
    static constexpr ft_status failure(int32_t error_code) noexcept;
    constexpr ft_bool is_success() const noexcept;
    constexpr ft_bool is_error() const noexcept;
};

template <typename ValueType>
class ft_result
{
    private:
        int32_t _error_code;
        ft_bool _has_value;
        alignas(ValueType) unsigned char _storage[sizeof(ValueType)];

        ValueType *value_pointer() noexcept;
        const ValueType *value_pointer() const noexcept;
        void destroy_value() noexcept;

    public:
        ft_result() noexcept;
        ft_result(const ft_result &other) = delete;
        ft_result(ft_result &&other) = delete;
        ~ft_result() noexcept;

        ft_result &operator=(const ft_result<ValueType> &other) noexcept = delete;
        ft_result &operator=(ft_result<ValueType> &&other) noexcept = delete;

        static ft_result<ValueType> *success(const ValueType &value) noexcept;
        static ft_result<ValueType> *success(ValueType &&value) noexcept;
        static ft_result<ValueType> *failure(int32_t error_code) noexcept;

        int32_t initialize() noexcept;
        int32_t initialize_value(const ValueType &value) noexcept;
        int32_t initialize_value(ValueType &&value) noexcept;
        int32_t initialize_error(int32_t error_code) noexcept;
        int32_t destroy() noexcept;

        ft_bool has_value() const noexcept;
        ft_bool is_success() const noexcept;
        ft_bool is_error() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
        ValueType *get_value() noexcept;
        const ValueType *get_value() const noexcept;
};

constexpr ft_status ft_status::success() noexcept
{
    return (ft_status{FT_ERR_SUCCESS});
}

constexpr ft_status ft_status::failure(int32_t error_code) noexcept
{
    if (error_code == FT_ERR_SUCCESS)
        return (ft_status{FT_ERR_INVALID_ARGUMENT});
    return (ft_status{error_code});
}

constexpr ft_bool ft_status::is_success() const noexcept
{
    if (this->error_code == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

constexpr ft_bool ft_status::is_error() const noexcept
{
    if (this->error_code != FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

template <typename ValueType>
ValueType *ft_result<ValueType>::value_pointer() noexcept
{
    return (reinterpret_cast<ValueType *>(this->_storage));
}

template <typename ValueType>
const ValueType *ft_result<ValueType>::value_pointer() const noexcept
{
    return (reinterpret_cast<const ValueType *>(this->_storage));
}

template <typename ValueType>
void ft_result<ValueType>::destroy_value() noexcept
{
    if (this->_has_value == FT_FALSE)
        return ;
    destroy_at(this->value_pointer());
    this->_has_value = FT_FALSE;
    return ;
}

template <typename ValueType>
ft_result<ValueType>::ft_result() noexcept
    : _error_code(FT_ERR_SUCCESS), _has_value(FT_FALSE)
{
    return ;
}

template <typename ValueType>
ft_result<ValueType>::~ft_result() noexcept
{
    this->destroy_value();
    return ;
}

template <typename ValueType>
ft_result<ValueType> *ft_result<ValueType>::success(const ValueType &value) noexcept
{
    ft_result<ValueType> *result;

    result = new (std::nothrow) ft_result<ValueType>();
    if (result == ft_nullptr)
        return (ft_nullptr);
    if (result->initialize_value(value) != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

template <typename ValueType>
ft_result<ValueType> *ft_result<ValueType>::success(ValueType &&value) noexcept
{
    ft_result<ValueType> *result;

    result = new (std::nothrow) ft_result<ValueType>();
    if (result == ft_nullptr)
        return (ft_nullptr);
    if (result->initialize_value(ft_move(value)) != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

template <typename ValueType>
ft_result<ValueType> *ft_result<ValueType>::failure(int32_t error_code) noexcept
{
    ft_result<ValueType> *result;
    int32_t stored_error;

    result = new (std::nothrow) ft_result<ValueType>();
    if (result == ft_nullptr)
        return (ft_nullptr);
    stored_error = result->initialize_error(error_code);
    if (stored_error == FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

template <typename ValueType>
int32_t ft_result<ValueType>::initialize() noexcept
{
    this->destroy_value();
    this->_error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_result<ValueType>::initialize_value(const ValueType &value) noexcept
{
    this->destroy_value();
    construct_at(this->value_pointer(), value);
    this->_has_value = FT_TRUE;
    this->_error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_result<ValueType>::initialize_value(ValueType &&value) noexcept
{
    this->destroy_value();
    construct_at(this->value_pointer(), ft_move(value));
    this->_has_value = FT_TRUE;
    this->_error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
int32_t ft_result<ValueType>::initialize_error(int32_t error_code) noexcept
{
    this->destroy_value();
    if (error_code == FT_ERR_SUCCESS)
        this->_error_code = FT_ERR_INVALID_ARGUMENT;
    else
        this->_error_code = error_code;
    return (this->_error_code);
}

template <typename ValueType>
int32_t ft_result<ValueType>::destroy() noexcept
{
    this->destroy_value();
    this->_error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

template <typename ValueType>
ft_bool ft_result<ValueType>::has_value() const noexcept
{
    return (this->_has_value);
}

template <typename ValueType>
ft_bool ft_result<ValueType>::is_success() const noexcept
{
    if (this->_error_code == FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

template <typename ValueType>
ft_bool ft_result<ValueType>::is_error() const noexcept
{
    if (this->_error_code != FT_ERR_SUCCESS)
        return (FT_TRUE);
    return (FT_FALSE);
}

template <typename ValueType>
int32_t ft_result<ValueType>::get_error() const noexcept
{
    return (this->_error_code);
}

template <typename ValueType>
const char *ft_result<ValueType>::get_error_str() const noexcept
{
    return (ft_strerror(this->_error_code));
}

template <typename ValueType>
ValueType *ft_result<ValueType>::get_value() noexcept
{
    if (this->_has_value == FT_FALSE)
        return (ft_nullptr);
    return (this->value_pointer());
}

template <typename ValueType>
const ValueType *ft_result<ValueType>::get_value() const noexcept
{
    if (this->_has_value == FT_FALSE)
        return (ft_nullptr);
    return (this->value_pointer());
}

#endif
