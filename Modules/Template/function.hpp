#ifndef TEMPLATE_FUNCTION_HPP
#define TEMPLATE_FUNCTION_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "move.hpp"

#include <new>
#include <type_traits>
#include <utility>

template <typename Signature>
class ft_function;

template <typename ReturnType, typename... Args>
class ft_function<ReturnType(Args...)>
{
    private:
        template <typename ArgumentType>
        using dispatch_argument = std::conditional_t<std::is_scalar_v<ArgumentType>
            || std::is_reference_v<ArgumentType>, ArgumentType, ArgumentType &>;

        template <typename ArgumentType>
        using callable_argument = std::conditional_t<std::is_rvalue_reference_v<ArgumentType>,
            ArgumentType, ArgumentType &>;

        void *_callable;
        ReturnType (*_invoke)(void *, dispatch_argument<Args>...);
        void (*_destroy)(void *);
        void *(*_clone)(void *);

        void    clear_callable();

        template <typename FunctionType>
        static ReturnType invoke(void *callable, dispatch_argument<Args>... args);

        template <typename FunctionType>
        static void destroy(void *callable);

        template <typename FunctionType>
        static void *clone(void *callable);

    public:
        ft_function();

        template <typename FunctionType>
        ft_function(FunctionType function);

        ft_function(const ft_function &other);
        ft_function(ft_function &&other) noexcept;
        ~ft_function();

        ft_function &operator=(const ft_function &other);
        ft_function &operator=(ft_function &&other) noexcept;

        ReturnType   operator()(Args... args) const;
        operator ft_bool() const noexcept;
};

template <typename ReturnType, typename... Args>
void ft_function<ReturnType(Args...)>::clear_callable()
{
    if (this->_destroy && this->_callable)
    {
        this->_destroy(this->_callable);
    }
    this->_callable = ft_nullptr;
    this->_invoke = ft_nullptr;
    this->_destroy = ft_nullptr;
    this->_clone = ft_nullptr;
    return ;
}

template <typename ReturnType, typename... Args>
template <typename FunctionType>
ReturnType ft_function<ReturnType(Args...)>::invoke(void *callable,
    dispatch_argument<Args>... args)
{
    FunctionType *function;

    function = static_cast<FunctionType *>(callable);
    return ((*function)(std::forward<callable_argument<Args>>(args)...));
}

template <typename ReturnType, typename... Args>
template <typename FunctionType>
void ft_function<ReturnType(Args...)>::destroy(void *callable)
{
    FunctionType *function;

    function = static_cast<FunctionType *>(callable);
    delete function;
    return ;
}

template <typename ReturnType, typename... Args>
template <typename FunctionType>
void *ft_function<ReturnType(Args...)>::clone(void *callable)
{
    FunctionType *function;
    FunctionType *copy;

    function = static_cast<FunctionType *>(callable);
    copy = new (std::nothrow) FunctionType(*function);
    return (copy);
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)>::ft_function()
    : _callable(ft_nullptr), _invoke(ft_nullptr), _destroy(ft_nullptr),
      _clone(ft_nullptr)
{
    return ;
}

template <typename ReturnType, typename... Args>
template <typename FunctionType>
ft_function<ReturnType(Args...)>::ft_function(FunctionType function)
    : _callable(ft_nullptr), _invoke(ft_nullptr), _destroy(ft_nullptr),
      _clone(ft_nullptr)
{
    FunctionType *copy;

    copy = new (std::nothrow) FunctionType(ft_move(function));
    if (copy == ft_nullptr)
    {
        return ;
    }
    this->_callable = copy;
    this->_invoke = &ft_function<ReturnType(Args...)>::template invoke<FunctionType>;
    this->_destroy = &ft_function<ReturnType(Args...)>::template destroy<FunctionType>;
    this->_clone = &ft_function<ReturnType(Args...)>::template clone<FunctionType>;
    return ;
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)>::ft_function(const ft_function &other)
    : _callable(ft_nullptr), _invoke(ft_nullptr), _destroy(ft_nullptr),
      _clone(ft_nullptr)
{
    if (other._callable == ft_nullptr)
        return ;
    void *new_callable;

    new_callable = other._clone(other._callable);
    if (new_callable == ft_nullptr)
        return ;
    this->_callable = new_callable;
    this->_invoke = other._invoke;
    this->_destroy = other._destroy;
    this->_clone = other._clone;
    return ;
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)>::ft_function(ft_function &&other) noexcept
    : _callable(other._callable), _invoke(other._invoke),
      _destroy(other._destroy), _clone(other._clone)
{
    other._callable = ft_nullptr;
    other._invoke = ft_nullptr;
    other._destroy = ft_nullptr;
    other._clone = ft_nullptr;
    return ;
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)>::~ft_function()
{
    this->clear_callable();
    return ;
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)> &ft_function<ReturnType(Args...)>::operator=(const ft_function &other)
{
    if (this == &other)
        return (*this);
    void *new_callable;

    new_callable = ft_nullptr;
    if (other._callable != ft_nullptr)
        new_callable = other._clone(other._callable);
    this->clear_callable();
    if (new_callable == ft_nullptr)
        return (*this);
    this->_callable = new_callable;
    this->_invoke = other._invoke;
    this->_destroy = other._destroy;
    this->_clone = other._clone;
    return (*this);
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)> &ft_function<ReturnType(Args...)>::operator=(ft_function &&other) noexcept
{
    if (this == &other)
        return (*this);
    this->clear_callable();
    this->_callable = other._callable;
    this->_invoke = other._invoke;
    this->_destroy = other._destroy;
    this->_clone = other._clone;
    other._callable = ft_nullptr;
    other._invoke = ft_nullptr;
    other._destroy = ft_nullptr;
    other._clone = ft_nullptr;
    return (*this);
}

template <typename ReturnType, typename... Args>
ReturnType ft_function<ReturnType(Args...)>::operator()(Args... args) const
{
    ReturnType (*invoke_target)(void *, dispatch_argument<Args>...);
    void *callable;

    invoke_target = this->_invoke;
    callable = this->_callable;
    if (invoke_target == ft_nullptr || callable == ft_nullptr)
    {
        if constexpr (std::is_void<ReturnType>::value)
        {
            return ;
        }
        return (ReturnType());
    }
    return (invoke_target(callable, std::forward<dispatch_argument<Args>>(args)...));
}

template <typename ReturnType, typename... Args>
ft_function<ReturnType(Args...)>::operator ft_bool() const noexcept
{
    return (this->_callable != ft_nullptr && this->_invoke != ft_nullptr);
}

#endif
