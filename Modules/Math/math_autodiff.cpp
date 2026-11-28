#include "ft_dual_number.hpp"

#include <cmath>
#include <functional>
#include <new>
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/vector.hpp"

thread_local int32_t ft_dual_number::_last_error = FT_ERR_SUCCESS;

static void math_autodiff_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

void ft_dual_number::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

void ft_dual_number::abort_if_not_initialised(const char *method_name) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, method_name);
    return ;
}

int32_t ft_dual_number::set_error(int32_t error_code) noexcept
{
    ft_dual_number::_last_error = error_code;
    return (error_code);
}

int32_t ft_dual_number::lock_pair(const ft_dual_number &first,
    const ft_dual_number &second,
    const ft_dual_number *&lower,
    const ft_dual_number *&upper)
{
    const ft_dual_number *ordered_first;
    const ft_dual_number *ordered_second;

    if (first._mutex == ft_nullptr && second._mutex == ft_nullptr)
    {
        lower = ft_nullptr;
        upper = ft_nullptr;
        return (FT_ERR_SUCCESS);
    }
    if (&first == &second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    ordered_first = &first;
    ordered_second = &second;
    if (std::less<const ft_dual_number *>{}(ordered_second, ordered_first))
    {
        const ft_dual_number *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
    }
    lower = ordered_first;
    upper = ordered_second;
    while (FT_TRUE)
    {
        int32_t lower_error;
        int32_t upper_error;

        lower_error = pt_recursive_mutex_lock_if_not_null(lower->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
            return (lower_error);
        upper_error = pt_recursive_mutex_lock_if_not_null(upper->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
            return (upper_error);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
        math_autodiff_sleep_backoff();
    }
}

void ft_dual_number::unlock_pair(const ft_dual_number *lower,
    const ft_dual_number *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

ft_dual_number::ft_dual_number() noexcept
    : _value(0.0)
    , _derivative(0.0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _operation_error(FT_ERR_SUCCESS)
{
    return ;
}

ft_dual_number::ft_dual_number(double value, double derivative) noexcept
    : _value(0.0)
    , _derivative(0.0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _operation_error(FT_ERR_SUCCESS)
{
    int32_t previous_last_error;

    previous_last_error = ft_dual_number::_last_error;
    this->_operation_error = this->initialize(value, derivative);
    if (this->_operation_error != FT_ERR_SUCCESS
        && this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_dual_number::set_error(previous_last_error);
    return ;
}

int32_t ft_dual_number::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        this->abort_lifecycle_error("ft_dual_number::initialize",
            "called while object is already initialised");
        return (ft_dual_number::set_error(FT_ERR_INVALID_STATE));
    }
    this->_value = 0.0;
    this->_derivative = 0.0;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_dual_number::initialize(double value, double derivative) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (ft_dual_number::set_error(static_cast<uint32_t>(initialize_error)));
    this->_value = value;
    this->_derivative = derivative;
    this->_operation_error = FT_ERR_SUCCESS;
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_dual_number::initialize(const ft_dual_number &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        other.abort_lifecycle_error("ft_dual_number::initialize(const ft_dual_number &) source",
            "called with uninitialised source object");
        return (ft_dual_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (ft_dual_number::set_error(static_cast<uint32_t>(destroy_error)));
        }
        this->_value = 0.0;
        this->_derivative = 0.0;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (ft_dual_number::set_error(static_cast<uint32_t>(destroy_error)));
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (ft_dual_number::set_error(static_cast<uint32_t>(initialize_error)));
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->destroy();
        return (ft_dual_number::set_error(static_cast<uint32_t>(lock_error)));
    }
    this->_value = other._value;
    this->_derivative = other._derivative;
    this->_operation_error = other._operation_error;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_dual_number::move(ft_dual_number &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        other.abort_lifecycle_error("ft_dual_number::move source",
            "called with uninitialised source object");
        return (ft_dual_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_value = 0.0;
        this->_derivative = 0.0;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_value = 0.0;
        this->_derivative = 0.0;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_dual_number::set_error(lock_error));
    }
    this->_value = other._value;
    this->_derivative = other._derivative;
    this->_operation_error = other._operation_error;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._value = 0.0;
    other._derivative = 0.0;
    other._operation_error = FT_ERR_SUCCESS;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_dual_number::initialize(ft_dual_number &&other) noexcept
{
    return (this->move(other));
}

int32_t ft_dual_number::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    this->_value = 0.0;
    this->_derivative = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (first_error != FT_ERR_SUCCESS)
    {
        this->_operation_error = first_error;
        return (ft_dual_number::set_error(static_cast<uint32_t>(first_error)));
    }
    this->_operation_error = FT_ERR_SUCCESS;
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

ft_dual_number::~ft_dual_number() noexcept
{
    int32_t previous_last_error;

    previous_last_error = ft_dual_number::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    ft_dual_number::set_error(previous_last_error);
    return ;
}

ft_dual_number *ft_dual_number::constant(double value) noexcept
{
    ft_dual_number *result;
    int32_t initialize_error;

    result = new (std::nothrow) ft_dual_number();
    if (result == ft_nullptr)
    {
        ft_dual_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    initialize_error = result->initialize(value, 0.0);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)result->destroy();
        delete result;
        ft_dual_number::set_error(initialize_error);
        return (ft_nullptr);
    }
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_dual_number *ft_dual_number::variable(double value) noexcept
{
    ft_dual_number *result;
    int32_t initialize_error;

    result = new (std::nothrow) ft_dual_number();
    if (result == ft_nullptr)
    {
        ft_dual_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    initialize_error = result->initialize(value, 1.0);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)result->destroy();
        delete result;
        ft_dual_number::set_error(initialize_error);
        return (ft_nullptr);
    }
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t ft_dual_number::enable_thread_safety() noexcept
{
    this->abort_if_not_initialised("ft_dual_number::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_dual_number::set_error(FT_ERR_NO_MEMORY));
    const int32_t error = mutex_pointer->initialize();
    if (error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (ft_dual_number::set_error(error));
    }
    this->_mutex = mutex_pointer;
    return (ft_dual_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_dual_number::disable_thread_safety() noexcept
{
    this->abort_if_not_initialised("ft_dual_number::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (ft_dual_number::set_error(FT_ERR_SUCCESS));
    const int32_t error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (ft_dual_number::set_error(error));
}

ft_bool ft_dual_number::is_thread_safe() const noexcept
{
    this->abort_if_not_initialised("ft_dual_number::is_thread_safe");
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

double ft_dual_number::value() const noexcept
{
    int32_t lock_error;
    double result;

    this->abort_if_not_initialised("ft_dual_number::value");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_dual_number::set_error(lock_error);
        return (0.0);
    }
    result = this->_value;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

double ft_dual_number::derivative() const noexcept
{
    int32_t lock_error;
    double result;

    this->abort_if_not_initialised("ft_dual_number::derivative");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_dual_number::set_error(lock_error);
        return (0.0);
    }
    result = this->_derivative;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_dual_number_proxy::ft_dual_number_proxy() noexcept
    : _value(0.0, 0.0)
    , _operation_error(FT_ERR_SUCCESS)
{
    return ;
}

ft_dual_number_proxy::ft_dual_number_proxy(int32_t error_code) noexcept
    : _value(0.0, 0.0)
    , _operation_error(error_code)
{
    this->_value._operation_error = error_code;
    ft_dual_number::set_error(error_code);
    return ;
}

ft_dual_number_proxy::ft_dual_number_proxy(const ft_dual_number &value,
    int32_t error_code) noexcept
    : _value()
    , _operation_error(error_code)
{
    const int32_t copy_error = this->_value.initialize(value);
    if (copy_error != FT_ERR_SUCCESS)
        this->_operation_error = copy_error;
    this->_value._operation_error = this->_operation_error;
    ft_dual_number::set_error(this->_operation_error);
    return ;
}

ft_dual_number_proxy::ft_dual_number_proxy(
    const ft_dual_number_proxy &other) noexcept
    : _value()
    , _operation_error(other._operation_error)
{
    if (this->_value.initialize(other._value) != FT_ERR_SUCCESS)
        this->_operation_error = this->_value._operation_error;
    return ;
}

ft_dual_number_proxy::ft_dual_number_proxy(
    ft_dual_number_proxy &&other) noexcept
    : _value()
    , _operation_error(other._operation_error)
{
    if (this->_value.initialize(static_cast<ft_dual_number &&>(other._value))
        != FT_ERR_SUCCESS)
        this->_operation_error = this->_value._operation_error;
    other._operation_error = FT_ERR_SUCCESS;
    return ;
}

ft_dual_number_proxy::~ft_dual_number_proxy()
{
    return ;
}

ft_dual_number_proxy ft_dual_number_proxy::operator+(
    const ft_dual_number &other) const noexcept
{
    return (this->_value + other);
}

ft_dual_number_proxy ft_dual_number_proxy::operator-(
    const ft_dual_number &other) const noexcept
{
    return (this->_value - other);
}

ft_dual_number_proxy ft_dual_number_proxy::operator*(
    const ft_dual_number &other) const noexcept
{
    return (this->_value * other);
}

ft_dual_number_proxy ft_dual_number_proxy::operator/(
    const ft_dual_number &other) const noexcept
{
    return (this->_value / other);
}

ft_dual_number_proxy::operator ft_dual_number *() const noexcept
{
    ft_dual_number *result;

    result = new (std::nothrow) ft_dual_number();
    if (result == ft_nullptr)
    {
        ft_dual_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (result->initialize(this->_value) != FT_ERR_SUCCESS)
    {
        (void)result->destroy();
        delete result;
        ft_dual_number::set_error(FT_ERR_INITIALIZATION_FAILED);
        return (ft_nullptr);
    }
    result->_operation_error = this->_operation_error;
    ft_dual_number::set_error(this->_operation_error);
    return (result);
}

int32_t ft_dual_number_proxy::get_error() const noexcept
{
    return (this->_operation_error);
}

ft_dual_number_proxy::ft_dual_number_proxy(double value, double derivative) noexcept
    : _value(value, derivative)
    , _operation_error(FT_ERR_SUCCESS)
{
    ft_dual_number::set_error(FT_ERR_SUCCESS);
    return ;
}

static double math_dual_scaled_quotient_derivative(double left_value, double right_value,
    double left_derivative, double right_derivative) noexcept
{
    int32_t denominator_exponent;
    int32_t numerator_exponent;
    int32_t left_derivative_exponent;
    int32_t right_derivative_exponent;
    const double denominator = std::frexp(right_value, &denominator_exponent);
    const double numerator = std::frexp(left_value, &numerator_exponent);
    const double left = std::frexp(left_derivative, &left_derivative_exponent);
    const double right = std::frexp(right_derivative, &right_derivative_exponent);

    const double first = left * denominator;
    const double second = numerator * right;
    const int32_t first_exponent = left_derivative_exponent + denominator_exponent;
    const int32_t second_exponent = numerator_exponent + right_derivative_exponent;
    int32_t scale = first_exponent;
    if (std::fpclassify(first) == FP_ZERO
        || (std::fpclassify(second) != FP_ZERO && second_exponent > scale))
        scale = second_exponent;
    const double difference = std::scalbn(first, first_exponent - scale)
        - std::scalbn(second, second_exponent - scale);
    return (std::scalbn(difference / (denominator * denominator),
        scale - 2 * denominator_exponent));
}

static double math_dual_quotient_derivative(double left_value, double right_value,
    double left_derivative, double right_derivative) noexcept
{
    if (std::fpclassify(right_derivative) == FP_ZERO
        && std::isfinite(left_value) && std::isfinite(right_value))
        return (left_derivative / right_value);

    const double first = left_derivative * right_value;
    const double second = left_value * right_derivative;
    const double divisor = right_value * right_value;
    const double difference = first - second;
    if (((std::fpclassify(left_derivative) == FP_ZERO || std::isnormal(first))
            && (std::fpclassify(left_value) == FP_ZERO
                || std::fpclassify(right_derivative) == FP_ZERO || std::isnormal(second))
            && std::isfinite(difference) && std::isfinite(divisor))
        || !std::isfinite(left_value) || !std::isfinite(right_value)
        || !std::isfinite(left_derivative) || !std::isfinite(right_derivative))
        return (difference / divisor);
    return (math_dual_scaled_quotient_derivative(left_value, right_value,
        left_derivative, right_derivative));
}

template <char Operation>
ft_dual_number_proxy ft_dual_number::apply_binary(const ft_dual_number &other,
    const char *method_name) const noexcept
{
    this->abort_if_not_initialised(method_name);
    other.abort_if_not_initialised(method_name);
    const ft_dual_number *lower;
    const ft_dual_number *upper;
    int32_t error = ft_dual_number::lock_pair(*this, other, lower, upper);
    if (error != FT_ERR_SUCCESS)
        return (ft_dual_number_proxy(error));
    error = this->_operation_error;
    if (error == FT_ERR_SUCCESS)
        error = other._operation_error;
    const double left_value = this->_value;
    const double right_value = other._value;
    const double left_derivative = this->_derivative;
    const double right_derivative = other._derivative;
    ft_dual_number::unlock_pair(lower, upper);
    if (error != FT_ERR_SUCCESS)
        return (ft_dual_number_proxy(error));

    if constexpr (Operation == '+')
        return (ft_dual_number_proxy(left_value + right_value, left_derivative + right_derivative));
    else if constexpr (Operation == '-')
        return (ft_dual_number_proxy(left_value - right_value, left_derivative - right_derivative));
    else if constexpr (Operation == '*')
        return (ft_dual_number_proxy(left_value * right_value,
            left_value * right_derivative + left_derivative * right_value));
    else
    {
        constexpr double DENOMINATOR_EPSILON = 1e-12;
        if (std::fabs(right_value) <= DENOMINATOR_EPSILON)
            return (ft_dual_number_proxy(FT_ERR_DIVIDE_BY_ZERO));
        return (ft_dual_number_proxy(left_value / right_value,
            math_dual_quotient_derivative(left_value, right_value,
                left_derivative, right_derivative)));
    }
}

ft_dual_number_proxy ft_dual_number::operator+(const ft_dual_number &other) const noexcept
{
    return (this->apply_binary<'+'>(other, "ft_dual_number::operator+"));
}

ft_dual_number_proxy ft_dual_number::operator-(const ft_dual_number &other) const noexcept
{
    return (this->apply_binary<'-'>(other, "ft_dual_number::operator-"));
}

ft_dual_number_proxy ft_dual_number::operator*(const ft_dual_number &other) const noexcept
{
    return (this->apply_binary<'*'>(other, "ft_dual_number::operator*"));
}

ft_dual_number_proxy ft_dual_number::operator/(const ft_dual_number &other) const noexcept
{
    return (this->apply_binary<'/'>(other, "ft_dual_number::operator/"));
}

template <typename Operation>
ft_dual_number *ft_dual_number::apply_unary(Operation operation, const char *method_name) const noexcept
{
    double value = 0.0;
    double derivative_value = 0.0;
    int32_t error = this->read_result(value, derivative_value, method_name);

    ft_dual_number *result = new (std::nothrow) ft_dual_number();
    if (result == ft_nullptr)
    {
        ft_dual_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    const int32_t initialize_error = result->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_dual_number::set_error(initialize_error);
        return (ft_nullptr);
    }
    if (error == FT_ERR_SUCCESS)
        error = operation(value, derivative_value, result->_value, result->_derivative);
    result->_operation_error = error;
    ft_dual_number::set_error(error);
    return (result);
}

ft_dual_number *ft_dual_number::apply_sin() const noexcept
{
    return (this->apply_unary([](double value, double derivative, double &output,
        double &output_derivative) noexcept -> int32_t
    {
        output = std::sin(value);
        output_derivative = std::cos(value) * derivative;
        return (FT_ERR_SUCCESS);
    }, "ft_dual_number::apply_sin"));
}

ft_dual_number *ft_dual_number::apply_cos() const noexcept
{
    return (this->apply_unary([](double value, double derivative, double &output,
        double &output_derivative) noexcept -> int32_t
    {
        output = std::cos(value);
        output_derivative = -std::sin(value) * derivative;
        return (FT_ERR_SUCCESS);
    }, "ft_dual_number::apply_cos"));
}

ft_dual_number *ft_dual_number::apply_exp() const noexcept
{
    return (this->apply_unary([](double value, double derivative, double &output,
        double &output_derivative) noexcept -> int32_t
    {
        output = std::exp(value);
        output_derivative = output * derivative;
        return (FT_ERR_SUCCESS);
    }, "ft_dual_number::apply_exp"));
}

ft_dual_number *ft_dual_number::apply_log() const noexcept
{
    return (this->apply_unary([](double value, double derivative, double &output,
        double &output_derivative) noexcept -> int32_t
    {
        if (value <= 0.0)
            return (FT_ERR_INVALID_ARGUMENT);
        output = std::log(value);
        output_derivative = derivative / value;
        return (FT_ERR_SUCCESS);
    }, "ft_dual_number::apply_log"));
}

int32_t ft_dual_number::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        this->abort_lifecycle_error("ft_dual_number::get_error",
            "called while object is uninitialised");
    return (ft_dual_number::_last_error);
}

const char *ft_dual_number::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        this->abort_lifecycle_error("ft_dual_number::get_error_str",
            "called while object is uninitialised");
    return (ft_strerror(this->get_error()));
}

int32_t ft_dual_number::read_result(double &value, double &derivative,
    const char *method_name) const noexcept
{
    this->abort_if_not_initialised(method_name);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t operation_error = this->_operation_error;
    if (operation_error == FT_ERR_SUCCESS)
    {
        value = this->_value;
        derivative = this->_derivative;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (operation_error);
}

static int32_t math_autodiff_prepare_inputs(const ft_vector<double> &point,
    ft_vector<ft_dual_number> &dual_inputs) noexcept
{
    ft_bool acquired = FT_FALSE;
    int32_t error = point.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);

    const ft_size_t dimension = point.size();
    error = point.get_error();
    if (error == FT_ERR_SUCCESS)
    {
        dual_inputs.reserve(dimension);
        error = dual_inputs.get_error();
    }
    if (error == FT_ERR_SUCCESS && dimension != 0U)
    {
        const double *coordinates = point.begin();
        error = point.get_error();
        ft_size_t index = 0U;
        while (error == FT_ERR_SUCCESS && index < dimension)
        {
            ft_dual_number variable;
            error = variable.initialize(coordinates[index], 0.0);
            if (error == FT_ERR_SUCCESS)
                error = dual_inputs.push_back(variable);
            index += 1U;
        }
    }
    point.unlock(acquired);
    return (error);
}

int32_t math_autodiff_univariate(math_autodiff_univariate_function function,
    double point, double *value, double *derivative, void *user_data) noexcept
{
    if (function == ft_nullptr || value == ft_nullptr || derivative == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_dual_number variable;
    const int32_t initialize_error = variable.initialize(point, 1.0);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    ft_dual_number *result = function(variable, user_data);
    if (result == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    const int32_t error = result->read_result(*value, *derivative);
    delete result;
    return (error);
}

static int32_t math_autodiff_publish_gradient(ft_vector<double> &gradient,
    const double *pending, ft_size_t dimension) noexcept
{
    ft_bool acquired = FT_FALSE;
    int32_t error = gradient.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    gradient.resize(dimension);
    error = gradient.get_error();
    if (error == FT_ERR_SUCCESS && dimension != 0U)
    {
        double *output = gradient.begin();
        error = gradient.get_error();
        if (error == FT_ERR_SUCCESS)
            ft_memcpy(output, pending, dimension * sizeof(double));
    }
    gradient.unlock(acquired);
    return (error);
}

int32_t math_autodiff_gradient(math_autodiff_multivariate_function function,
    const ft_vector<double> &point, ft_vector<double> &gradient,
    double *value, void *user_data) noexcept
{
    if (function == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_vector<ft_dual_number> dual_inputs;
    int32_t error = dual_inputs.initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = math_autodiff_prepare_inputs(point, dual_inputs);
    if (error != FT_ERR_SUCCESS)
        return (error);
    const ft_size_t dimension = dual_inputs.size();
    if (dimension == 0U)
        return (math_autodiff_publish_gradient(gradient, ft_nullptr, 0U));

    ft_vector<double> pending;
    error = pending.initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    pending.resize(dimension);
    error = pending.get_error();
    if (error != FT_ERR_SUCCESS)
        return (error);

    ft_dual_number *inputs = dual_inputs.begin();
    double *partials = pending.begin();
    double function_value = 0.0;
    ft_size_t index = 0U;
    while (index < dimension)
    {
        inputs[index]._derivative = 1.0;
        ft_dual_number *result = function(dual_inputs, user_data);
        inputs[index]._derivative = 0.0;
        if (result == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        double sample_value = 0.0;
        error = result->read_result(sample_value, partials[index]);
        delete result;
        if (error != FT_ERR_SUCCESS)
            return (error);
        if (index == 0U)
            function_value = sample_value;
        index += 1U;
    }
    error = math_autodiff_publish_gradient(gradient, partials, dimension);
    if (error == FT_ERR_SUCCESS && value != ft_nullptr)
        *value = function_value;
    return (error);
}
