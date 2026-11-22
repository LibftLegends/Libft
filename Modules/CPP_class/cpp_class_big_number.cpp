#include "class_big_number.hpp"
#include "class_string.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/move.hpp"
#include <limits>
#include <new>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_big_number::_last_error = FT_ERR_SUCCESS;
thread_local uint8_t ft_big_number::_last_initialised_state = FT_CLASS_STATE_UNINITIALISED;

struct ft_big_number_digit_entry
{
    char    symbol;
    int32_t     value;
};

static const ft_big_number_digit_entry g_big_number_digit_to_value_table[] =
{
    {'0', 0},
    {'1', 1},
    {'2', 2},
    {'3', 3},
    {'4', 4},
    {'5', 5},
    {'6', 6},
    {'7', 7},
    {'8', 8},
    {'9', 9},
    {'A', 10},
    {'B', 11},
    {'C', 12},
    {'D', 13},
    {'E', 14},
    {'F', 15},
    {'a', 10},
    {'b', 11},
    {'c', 12},
    {'d', 13},
    {'e', 14},
    {'f', 15},
    {'\0', -1}
};

static const char g_big_number_value_to_digit_table[] = "0123456789ABCDEF";

static int32_t ft_big_number_digit_value(char digit) noexcept
{
    ft_size_t index = 0;

    while (g_big_number_digit_to_value_table[index].symbol != '\0')
    {
        if (g_big_number_digit_to_value_table[index].symbol == digit)
            return (g_big_number_digit_to_value_table[index].value);
        index++;
    }
    return (-1);
}

static char ft_big_number_digit_symbol(int32_t value) noexcept
{
    if (value < 0 || value > 15)
        return ('\0');
    return (g_big_number_value_to_digit_table[value]);
}

int32_t ft_big_number::lock_pair(const ft_big_number &first, const ft_big_number &second,
        const ft_big_number *&lower,
        const ft_big_number *&upper) noexcept
{
    lower = &first;
    upper = &second;
    if (&first == &second)
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    if (lower > upper)
    {
        const ft_big_number *temporary = lower;

        lower = upper;
        upper = temporary;
    }
    while (FT_TRUE)
    {
        int32_t lower_error = pt_recursive_mutex_lock_if_not_null(lower->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
            return (lower_error);
        int32_t upper_error = pt_recursive_mutex_lock_if_not_null(upper->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
            return (upper_error);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
        ft_big_number::sleep_backoff();
    }
}

int32_t ft_big_number::unlock_pair(const ft_big_number *lower, const ft_big_number *upper) noexcept
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::enable_thread_safety(void) noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "enable thread safety");
    if (this->_mutex != ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_big_number::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (ft_big_number::set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_big_number::disable_thread_safety(void) noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "disable thread safety");
    if (this->_mutex == ft_nullptr)
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (ft_big_number::set_error(destroy_error));
}

ft_bool ft_big_number::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "is thread safe");
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

void ft_big_number::sleep_backoff() noexcept
{
    pt_thread_sleep(1);
    return ;
}

int32_t ft_big_number::set_error(int32_t error_code) noexcept
{
    ft_big_number::_last_error = error_code;
    return (error_code);
}

int32_t ft_big_number::get_error() noexcept
{
    errno_abort_if_uninitialised(ft_big_number::_last_initialised_state,
        "ft_big_number::get_error");
    return (ft_big_number::_last_error);
}

const char *ft_big_number::get_error_str() noexcept
{
    errno_abort_if_uninitialised(ft_big_number::_last_initialised_state,
        "ft_big_number::get_error_str");
    const char* error_string = ft_strerror(ft_big_number::_last_error);

    if (!error_string || error_string[0] == '\0')
        error_string = "unknown error";
    return (error_string);
}

void ft_big_number::clear_unlocked() noexcept
{
    this->_size = 0;
    if (this->_digits)
        this->_digits[0] = '\0';
    this->_is_negative = FT_FALSE;
    this->shrink_capacity();
    return ;
}

void ft_big_number::append_digit_unlocked(char digit) noexcept
{
    ft_size_t required_capacity;
    ft_size_t new_capacity;

    if (digit < '0' || digit > '9')
        return ;
    required_capacity = this->_size + 2;
    if (required_capacity > this->_capacity || !this->_digits)
    {
        new_capacity = this->_capacity;
        if (new_capacity < required_capacity)
            new_capacity = required_capacity;
        this->reserve(new_capacity);
    }
    if (!this->_digits)
    {
        this->reserve(required_capacity);
    }
    if (!this->_digits)
        return ;
    if (this->_size == 1 && this->_digits[0] == '0')
        this->_size = 0;
    this->_digits[this->_size] = digit;
    this->_size++;
    this->_digits[this->_size] = '\0';
    return ;
}

void ft_big_number::append_unlocked(const char* digits) noexcept
{
    ft_size_t digit_index;

    if (!digits)
        return ;
    digit_index = 0;
    while (digits[digit_index] != '\0')
    {
        this->append_digit_unlocked(digits[digit_index]);
        digit_index++;
    }
    return ;
}

void ft_big_number::append_unsigned_unlocked(uint64_t value) noexcept
{
    char digit_buffer[32];
    ft_size_t buffer_index;

    buffer_index = 0;
    if (value == 0)
    {
        digit_buffer[buffer_index] = '0';
        buffer_index++;
    }
    while (value > 0)
    {
        uint64_t remainder;

        remainder = value % 10;
        digit_buffer[buffer_index] = static_cast<char>('0' + remainder);
        buffer_index++;
        value /= 10;
    }
    while (buffer_index > 0)
    {
        buffer_index--;
        this->append_digit_unlocked(digit_buffer[buffer_index]);
    }
    return ;
}

void ft_big_number::trim_leading_zeros_unlocked() noexcept
{
    ft_size_t leading_index;
    ft_size_t new_size;

    if (!this->_digits || this->_size == 0)
        return ;
    leading_index = 0;
    while (leading_index + 1 < this->_size && this->_digits[leading_index] == '0')
        leading_index++;
    if (leading_index == 0)
        return ;
    new_size = this->_size - leading_index;
    ft_memmove(this->_digits, this->_digits + leading_index, new_size);
    this->_size = new_size;
    this->_digits[this->_size] = '\0';
    this->shrink_capacity();
    return ;
}

void ft_big_number::reduce_to_unlocked(ft_size_t new_size) noexcept
{
    if (new_size >= this->_size)
        return ;
    this->_size = new_size;
    if (this->_digits)
    {
        this->_digits[this->_size] = '\0';
        if (this->_size == 0)
            this->_digits[0] = '\0';
    }
    this->shrink_capacity();
    return ;
}

ft_big_number::ft_big_number() noexcept
    : _digits(ft_nullptr)
    , _size(0)
    , _capacity(0)
    , _is_negative(FT_FALSE)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _operation_error(FT_ERR_SUCCESS)
{
    ft_big_number::_last_initialised_state = this->_initialised_state;
    return ;
}

int32_t ft_big_number::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_big_number::initialize",
            "called while object is already initialised");
        return (ft_big_number::set_error(FT_ERR_INVALID_STATE));
    }
    this->_digits = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_is_negative = FT_FALSE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_big_number::initialize(const ft_big_number& other) noexcept
{
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_big_number::initialize(const ft_big_number &) source",
            "called with uninitialised source object");
        return (ft_big_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (ft_big_number::set_error(destroy_error));
        }
        this->_digits = ft_nullptr;
        this->_size = 0;
        this->_capacity = 0;
        this->_is_negative = FT_FALSE;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }
    if (this == &other)
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (ft_big_number::set_error(destroy_error));
    }
    this->_digits = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_is_negative = FT_FALSE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    *this = other;
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_big_number::initialize(ft_big_number&& other) noexcept
{
    int32_t destroy_error;
    int32_t move_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_big_number::initialize(ft_big_number &&) source",
            "called with uninitialised source object");
        return (ft_big_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (ft_big_number::set_error(destroy_error));
        }
        this->_digits = ft_nullptr;
        this->_size = 0;
        this->_capacity = 0;
        this->_is_negative = FT_FALSE;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }
    if (this == &other)
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (ft_big_number::set_error(destroy_error));
    }
    this->_digits = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_is_negative = FT_FALSE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    move_error = this->move(other);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (ft_big_number::set_error(move_error));
    }
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_big_number::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }
    disable_error = this->disable_thread_safety();
    cma_free(this->_digits);
    this->_digits = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_is_negative = FT_FALSE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    if (disable_error != FT_ERR_SUCCESS)
        return (ft_big_number::set_error(disable_error));
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

ft_big_number::~ft_big_number() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return ;
    }
    (void)this->destroy();
    return ;
}

ft_big_number& ft_big_number::operator=(const ft_big_number& other) noexcept
{
    if (this == &other)
    {
        ft_big_number::set_error(FT_ERR_SUCCESS);
        return (*this);
    }
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t lock_error = ft_big_number::lock_pair(*this, other, lower, upper);

    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (*this);
    }
    char *new_digits = ft_nullptr;
    if (other._digits && other._capacity > 0)
    {
        new_digits = static_cast<char*>(cma_malloc(other._capacity * sizeof(char)));
        if (!new_digits)
        {
            ft_big_number::unlock_pair(lower, upper);
            ft_big_number::set_error(FT_ERR_NO_MEMORY);
            return (*this);
        }
        ft_bzero(new_digits, other._capacity * sizeof(char));
        ft_memcpy(new_digits, other._digits, other._size + 1);
    }
    cma_free(this->_digits);
    this->_digits = new_digits;
    this->_size = other._size;
    this->_capacity = other._capacity;
    this->_is_negative = other._is_negative;
    ft_big_number::unlock_pair(lower, upper);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (*this);
}

ft_big_number& ft_big_number::operator=(ft_big_number&& other) noexcept
{
    int32_t move_error = this->move(other);

    ft_big_number::set_error(move_error);
    return (*this);
}

ft_big_number& ft_big_number::operator=(const ft_big_number_proxy& other) noexcept
{
    ft_big_number *other_value;

    other_value = static_cast<ft_big_number *>(other);
    if (other_value == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (*this);
    }
    *this = *other_value;
    (void)other_value->destroy();
    delete other_value;
    return (*this);
}

int32_t ft_big_number::move(ft_big_number& other) noexcept
{
    const ft_big_number *lower;
    const ft_big_number *upper;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_big_number::move source",
            "called with uninitialised source object");
        return (ft_big_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            int32_t destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (ft_big_number::set_error(destroy_error));
        }
        this->_digits = ft_nullptr;
        this->_size = 0;
        this->_capacity = 0;
        this->_is_negative = FT_FALSE;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        int32_t initialization_error = this->initialize();
        if (initialization_error != FT_ERR_SUCCESS)
            return (ft_big_number::set_error(initialization_error));
    }
    lower = ft_nullptr;
    upper = ft_nullptr;
    lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (lock_error);
    }
    cma_free(this->_digits);
    this->_digits = other._digits;
    this->_size = other._size;
    this->_capacity = other._capacity;
    this->_is_negative = other._is_negative;
    this->_operation_error = other._operation_error;
    this->_mutex = other._mutex;
    other._digits = ft_nullptr;
    other._size = 0;
    other._capacity = 0;
    other._is_negative = FT_FALSE;
    other._operation_error = FT_ERR_SUCCESS;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)ft_big_number::unlock_pair(lower, upper);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_big_number::is_zero_value() const noexcept
{
    if (this->_size == 0)
        return (FT_TRUE);
    if (!this->_digits)
        return (FT_TRUE);
    ft_size_t digit_index = 0;
    while (digit_index < this->_size)
    {
        if (this->_digits[digit_index] != '0')
            return (FT_FALSE);
        digit_index++;
    }
    return (FT_TRUE);
}

int32_t ft_big_number::compare_magnitude(const ft_big_number& other) const noexcept
{
    if (this->is_zero_value() && other.is_zero_value())
        return (0);
    if (this->is_zero_value())
        return (-1);
    if (other.is_zero_value())
        return (1);
    if (this->_size > other._size)
        return (1);
    if (this->_size < other._size)
        return (-1);
    ft_size_t digit_index = 0;
    while (digit_index < this->_size)
    {
        char left_digit = this->_digits[digit_index];
        char right_digit = other._digits[digit_index];
        if (left_digit > right_digit)
            return (1);
        if (left_digit < right_digit)
            return (-1);
        digit_index++;
    }
    return (0);
}

ft_big_number *ft_big_number::add_magnitude(const ft_big_number& other) const noexcept
{
    ft_big_number *result;
    int32_t result_initialization_error;

    result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    result_initialization_error = result->initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(result_initialization_error);
        return (ft_nullptr);
    }
    if (this->is_zero_value())
    {
        if (other._size > 0 && other._digits)
        {
            result->reserve(other._size + 1);
            ft_memcpy(result->_digits, other._digits, other._size + 1);
            result->_size = other._size;
            result->_digits[result->_size] = '\0';
        }
        else
        {
            result->_size = 0;
            if (result->_digits)
                result->_digits[0] = '\0';
        }
        result->_is_negative = FT_FALSE;
        return (result);
    }
    if (other.is_zero_value())
    {
        if (this->_size > 0 && this->_digits)
        {
            result->reserve(this->_size + 1);
            ft_memcpy(result->_digits, this->_digits, this->_size + 1);
            result->_size = this->_size;
            result->_digits[result->_size] = '\0';
        }
        else
        {
            result->_size = 0;
            if (result->_digits)
                result->_digits[0] = '\0';
        }
        result->_is_negative = FT_FALSE;
        return (result);
    }
    ft_size_t left_length = this->_size;
    ft_size_t right_length = other._size;
    ft_size_t max_length = left_length;
    if (max_length < right_length)
        max_length = right_length;
    max_length++;
    result->reserve(max_length + 1);
    result->_size = max_length;
    result->_digits[max_length] = '\0';
    ft_size_t left_index = left_length;
    ft_size_t right_index = right_length;
    ft_size_t write_index = max_length;
    int32_t carry_value = 0;
    while (left_index > 0 || right_index > 0 || carry_value > 0)
    {
        int32_t digit_sum = carry_value;
        if (left_index > 0)
        {
            left_index--;
            digit_sum += this->_digits[left_index] - '0';
        }
        if (right_index > 0)
        {
            right_index--;
            digit_sum += other._digits[right_index] - '0';
        }
        write_index--;
        result->_digits[write_index] = static_cast<char>('0' + (digit_sum % 10));
        carry_value = digit_sum / 10;
    }
    ft_size_t used_digits = max_length - write_index;
    if (used_digits == 0)
    {
        result->_digits[0] = '0';
        result->_digits[1] = '\0';
        result->_size = 1;
    }
    else
    {
        ft_memmove(result->_digits, result->_digits + write_index, used_digits);
        result->_digits[used_digits] = '\0';
        result->_size = used_digits;
    }
    result->shrink_capacity();
    result->_is_negative = FT_FALSE;
    return (result);
}

ft_big_number *ft_big_number::subtract_magnitude(const ft_big_number& other) const noexcept
{
    ft_big_number *result;
    int32_t result_initialization_error;

    result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    result_initialization_error = result->initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(result_initialization_error);
        return (ft_nullptr);
    }
    int32_t comparison = this->compare_magnitude(other);
    if (comparison < 0)
    {
        return (result);
    }
    if (comparison == 0)
        return (result);
    if (other.is_zero_value())
    {
        if (this->_size > 0 && this->_digits)
        {
            result->reserve(this->_size + 1);
            ft_memcpy(result->_digits, this->_digits, this->_size + 1);
            result->_size = this->_size;
            result->_digits[result->_size] = '\0';
        }
        else
        {
            result->_size = 0;
            if (result->_digits)
                result->_digits[0] = '\0';
        }
        result->_is_negative = FT_FALSE;
        return (result);
    }
    result->reserve(this->_size + 1);
    result->_size = this->_size;
    result->_digits[this->_size] = '\0';
    ft_size_t left_index = this->_size;
    ft_size_t right_index = other._size;
    int32_t borrow_value = 0;
    while (left_index > 0)
    {
        left_index--;
        int32_t left_digit_value = this->_digits[left_index] - '0';
        if (borrow_value > 0)
            left_digit_value--;
        if (right_index > 0)
        {
            right_index--;
            int32_t right_digit_value = other._digits[right_index] - '0';
            left_digit_value -= right_digit_value;
        }
        if (left_digit_value < 0)
        {
            left_digit_value += 10;
            borrow_value = 1;
        }
        else
            borrow_value = 0;
        result->_digits[left_index] = static_cast<char>('0' + left_digit_value);
    }
    result->trim_leading_zeros();
    result->_is_negative = FT_FALSE;
    return (result);
}

void ft_big_number::reserve(ft_size_t new_capacity) noexcept
{
    if (new_capacity <= this->_capacity)
        return ;
    if (new_capacity < this->_size + 1)
        new_capacity = this->_size + 1;
    if (new_capacity < 16)
        new_capacity = 16;
    char* new_digits;
    if (this->_digits)
        new_digits = static_cast<char*>(cma_realloc(this->_digits, new_capacity));
    else
        new_digits = static_cast<char*>(cma_malloc(new_capacity * sizeof(char)));
    if (!new_digits)
        return ;
    this->_digits = new_digits;
    this->_capacity = new_capacity;
    return ;
}

void ft_big_number::shrink_capacity() noexcept
{
    if (!this->_digits)
        return ;
    if (this->_size == 0)
    {
        cma_free(this->_digits);
        this->_digits = ft_nullptr;
        this->_capacity = 0;
        return ;
    }
    ft_size_t desired_capacity = this->_size + 1;
    if (desired_capacity < 16)
        desired_capacity = 16;
    ft_size_t shrink_threshold = desired_capacity;
    if (desired_capacity <= std::numeric_limits<ft_size_t>::max() / 2)
        shrink_threshold = desired_capacity * 2;
    else
        shrink_threshold = std::numeric_limits<ft_size_t>::max();
    if (this->_capacity <= shrink_threshold)
        return ;
    char* new_digits = static_cast<char*>(cma_realloc(this->_digits, desired_capacity));
    if (!new_digits)
        return ;
    this->_digits = new_digits;
    this->_capacity = desired_capacity;
    return ;
}

void ft_big_number::assign(const char* number) noexcept
{
    int32_t lock_error;
    ft_bool parsed_negative;
    ft_size_t start_index;
    ft_size_t length;
    ft_size_t index;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (!number)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    parsed_negative = FT_FALSE;
    start_index = 0;
    if (number[start_index] == '-')
    {
        parsed_negative = FT_TRUE;
        start_index++;
    }
    length = 0;
    index = start_index;
    while (number[index] != '\0')
    {
        if (number[index] < '0' || number[index] > '9')
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return ;
        }
        length++;
        index++;
    }
    if (length == 0)
    {
        if (parsed_negative)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return ;
        }
        this->clear_unlocked();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    if (length + 1 > this->_capacity || !this->_digits)
    {
        this->reserve(length + 1);
        if (!this->_digits)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return ;
        }
    }
    ft_memcpy(this->_digits, number + start_index, length);
    this->_digits[length] = '\0';
    this->_size = length;
    this->_is_negative = parsed_negative;
    this->trim_leading_zeros_unlocked();
    if (this->is_zero_value())
        this->_is_negative = FT_FALSE;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_big_number::assign_base(const char* digits, int32_t base) noexcept
{
    int32_t lock_error;
    int32_t local_error;
    ft_bool parsed_negative;
    ft_size_t index;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (lock_error);
    }
    local_error = FT_ERR_SUCCESS;
    parsed_negative = FT_FALSE;
    index = 0;
    if (!digits)
        local_error = FT_ERR_INVALID_ARGUMENT;
    else if (base < 2 || base > 16)
        local_error = FT_ERR_INVALID_ARGUMENT;
    else
    {
        if (digits[index] == '-')
        {
            parsed_negative = FT_TRUE;
            index++;
        }
        if (digits[index] == '\0')
            local_error = FT_ERR_INVALID_ARGUMENT;
        else
        {
            ft_big_number base_number;
            ft_big_number result;
            int32_t base_number_initialization_error;
            int32_t result_initialization_error;

            base_number_initialization_error = base_number.initialize();
            if (base_number_initialization_error != FT_ERR_SUCCESS)
            {
                local_error = base_number_initialization_error;
            }
            else
            {
                base_number.append_unsigned(static_cast<uint64_t>(base));
                if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                    local_error = ft_big_number::_last_error;
                else
                {
                    result_initialization_error = result.initialize();
                    if (result_initialization_error != FT_ERR_SUCCESS)
                    {
                        local_error = result_initialization_error;
                    }
                    else
                    {
                        while (digits[index] != '\0' && local_error == FT_ERR_SUCCESS)
                        {
                            int32_t digit_value;
                            ft_big_number product;
                            int32_t product_initialization_error;
                            ft_big_number addend;
                            int32_t addend_initialization_error;

                            digit_value = ft_big_number_digit_value(digits[index]);
                            if (digit_value < 0 || digit_value >= base)
                            {
                                local_error = FT_ERR_INVALID_ARGUMENT;
                                break ;
                            }
                            product_initialization_error = product.initialize();
                            if (product_initialization_error != FT_ERR_SUCCESS)
                            {
                                local_error = product_initialization_error;
                                break ;
                            }
                            product = result * base_number;
                            if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                            {
                                local_error = ft_big_number::_last_error;
                                break ;
                            }
                            result = product;
                            if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                            {
                                local_error = ft_big_number::_last_error;
                                break ;
                            }
                            addend_initialization_error = addend.initialize();
                            if (addend_initialization_error != FT_ERR_SUCCESS)
                            {
                                local_error = addend_initialization_error;
                                break ;
                            }
                            addend.append_unsigned(static_cast<uint64_t>(digit_value));
                            if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                            {
                                local_error = ft_big_number::_last_error;
                                break ;
                            }
                            ft_big_number sum;
                            int32_t sum_initialization_error;

                            sum_initialization_error = sum.initialize();
                            if (sum_initialization_error != FT_ERR_SUCCESS)
                            {
                                local_error = sum_initialization_error;
                                break ;
                            }
                            sum = result + addend;
                            if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                            {
                                local_error = ft_big_number::_last_error;
                                break ;
                            }
                            result = sum;
                            if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                            {
                                local_error = ft_big_number::_last_error;
                                break ;
                            }
                            index++;
                        }
                    }
                }
            }
            if (local_error == FT_ERR_SUCCESS)
            {
                if (parsed_negative && result.is_zero_value())
                    parsed_negative = FT_FALSE;
                if (result._size + 1 > this->_capacity || !this->_digits)
                {
                    this->reserve(result._size + 1);
                    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                        local_error = ft_big_number::_last_error;
                }
                if (local_error == FT_ERR_SUCCESS)
                {
                    if (result._size > 0 && result._digits)
                        ft_memcpy(this->_digits, result._digits, result._size + 1);
                    else if (this->_digits)
                        this->_digits[0] = '\0';
                    this->_size = result._size;
                    this->_is_negative = parsed_negative;
                    if (this->is_zero_value())
                        this->_is_negative = FT_FALSE;
                    if (this->_digits)
                        this->_digits[this->_size] = '\0';
                    this->shrink_capacity();
                }
            }
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (local_error != FT_ERR_SUCCESS)
        ft_big_number::set_error(local_error);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (local_error);
}

void ft_big_number::append_digit(char digit) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    this->append_digit_unlocked(digit);
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        int32_t stored_error;

        stored_error = ft_big_number::_last_error;
        ft_big_number::set_error(stored_error);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_big_number::append(const char* digits) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    if (!digits)
    {
        ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    this->append_unlocked(digits);
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        int32_t stored_error;

        stored_error = ft_big_number::_last_error;
        ft_big_number::set_error(stored_error);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_big_number::append_unsigned(uint64_t value) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    this->append_unsigned_unlocked(value);
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        int32_t stored_error;

        stored_error = ft_big_number::_last_error;
        ft_big_number::set_error(stored_error);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_big_number::trim_leading_zeros() noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    this->trim_leading_zeros_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_big_number::reduce_to(ft_size_t new_size) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    this->reduce_to_unlocked(new_size);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_big_number::clear() noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    this->clear_unlocked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

ft_big_number_proxy ft_big_number::operator+(const ft_big_number& other) const noexcept
{
    ft_big_number result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t lock_error;
    int32_t operation_error;
    int32_t result_initialization_error;

    operation_error = FT_ERR_SUCCESS;
    result_initialization_error = result.initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = result_initialization_error;
        ft_big_number::set_error(operation_error);
        goto finalize_add;
    }
    lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        operation_error = lock_error;
        ft_big_number::set_error(operation_error);
        goto finalize_add;
    }
    if (ft_big_number::_last_error != 0)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto finalize_add;
    }
    if (this->_is_negative == other._is_negative)
    {
        ft_big_number *magnitude_result;

        magnitude_result = this->add_magnitude(other);
        if (magnitude_result == ft_nullptr)
        {
            operation_error = ft_big_number::_last_error;
            goto finalize_add;
        }
        result = *magnitude_result;
        (void)magnitude_result->destroy();
        delete magnitude_result;
        if (ft_big_number::_last_error == FT_ERR_SUCCESS)
        {
            result._is_negative = this->_is_negative;
            if (result.is_zero_value())
                result._is_negative = FT_FALSE;
        }
    }
    else
    {
        int32_t comparison = this->compare_magnitude(other);

        if (comparison == 0)
        {
            goto finalize_add;
        }
        if (comparison > 0)
        {
            ft_big_number *magnitude_result;

            magnitude_result = this->subtract_magnitude(other);
            if (magnitude_result == ft_nullptr)
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_add;
            }
            result = *magnitude_result;
            (void)magnitude_result->destroy();
            delete magnitude_result;
            if (ft_big_number::_last_error == FT_ERR_SUCCESS)
                result._is_negative = this->_is_negative;
        }
        else
        {
            ft_big_number *magnitude_result;

            magnitude_result = other.subtract_magnitude(*this);
            if (magnitude_result == ft_nullptr)
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_add;
            }
            result = *magnitude_result;
            (void)magnitude_result->destroy();
            delete magnitude_result;
            if (ft_big_number::_last_error == FT_ERR_SUCCESS)
                result._is_negative = other._is_negative;
        }
        if (ft_big_number::_last_error == FT_ERR_SUCCESS
                && result.is_zero_value())
            result._is_negative = FT_FALSE;
    }
finalize_add:
    if (operation_error == FT_ERR_SUCCESS
            && ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
    }
    {
        (void)ft_big_number::unlock_pair(lower, upper);
        if (operation_error == FT_ERR_SUCCESS)
        {
            ft_big_number::set_error(FT_ERR_SUCCESS);
        }
    }
    if (operation_error != FT_ERR_SUCCESS)
        return (ft_big_number_proxy(operation_error));
    return (result);
}

ft_big_number_proxy ft_big_number::operator-(const ft_big_number& other) const noexcept
{
    ft_big_number result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t lock_error;
    int32_t operation_error = FT_ERR_SUCCESS;
    int32_t result_initialization_error;
    ft_bool left_negative;
    ft_bool right_negative;

    result_initialization_error = result.initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = result_initialization_error;
        ft_big_number::set_error(operation_error);
        goto finalize_subtract;
    }

    lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        operation_error = lock_error;
        ft_big_number::set_error(operation_error);
        goto finalize_subtract;
    }
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto finalize_subtract;
    }
    left_negative = this->_is_negative;
    right_negative = !other._is_negative;
    if (other.is_zero_value())
        right_negative = FT_FALSE;
    if (left_negative == right_negative)
    {
        ft_big_number *magnitude_result;

        magnitude_result = this->add_magnitude(other);
        if (magnitude_result == ft_nullptr)
        {
            operation_error = ft_big_number::_last_error;
            goto finalize_subtract;
        }
        result = *magnitude_result;
        (void)magnitude_result->destroy();
        delete magnitude_result;
        if (ft_big_number::_last_error == FT_ERR_SUCCESS)
        {
            result._is_negative = left_negative;
            if (result.is_zero_value())
                result._is_negative = FT_FALSE;
        }
        else
        {
            operation_error = ft_big_number::_last_error;
            goto finalize_subtract;
        }
    }
    else
    {
        int32_t comparison = this->compare_magnitude(other);

        if (comparison == 0)
        {
            goto finalize_subtract;
        }
        if (comparison > 0)
        {
            ft_big_number *magnitude_result;

            magnitude_result = this->subtract_magnitude(other);
            if (magnitude_result == ft_nullptr)
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_subtract;
            }
            result = *magnitude_result;
            (void)magnitude_result->destroy();
            delete magnitude_result;
            if (ft_big_number::_last_error == FT_ERR_SUCCESS)
                result._is_negative = left_negative;
            else
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_subtract;
            }
        }
        else
        {
            ft_big_number *magnitude_result;

            magnitude_result = other.subtract_magnitude(*this);
            if (magnitude_result == ft_nullptr)
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_subtract;
            }
            result = *magnitude_result;
            (void)magnitude_result->destroy();
            delete magnitude_result;
            if (ft_big_number::_last_error == FT_ERR_SUCCESS)
                result._is_negative = right_negative;
            else
            {
                operation_error = ft_big_number::_last_error;
                goto finalize_subtract;
            }
        }
        if (ft_big_number::_last_error == FT_ERR_SUCCESS
                && result.is_zero_value())
            result._is_negative = FT_FALSE;
    }
finalize_subtract:
    if (operation_error == FT_ERR_SUCCESS
            && ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
    }
    {
        (void)ft_big_number::unlock_pair(lower, upper);
        if (operation_error == FT_ERR_SUCCESS)
        {
            ft_big_number::set_error(FT_ERR_SUCCESS);
        }
    }
    if (operation_error != FT_ERR_SUCCESS)
        return (ft_big_number_proxy(operation_error));
    return (result);
}

ft_big_number_proxy ft_big_number::operator*(const ft_big_number& other) const noexcept
{
    ft_big_number result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t lock_error;
    int32_t operation_error = FT_ERR_SUCCESS;
    int32_t result_initialization_error;

    lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        operation_error = lock_error;
        ft_big_number::set_error(operation_error);
        goto finalize_multiply;
    }
    result_initialization_error = result.initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = result_initialization_error;
        ft_big_number::set_error(operation_error);
        goto finalize_multiply;
    }
    if (ft_big_number::_last_error != 0)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto finalize_multiply;
    }
    if (!this->is_zero_value() && !other.is_zero_value())
    {
        ft_size_t left_length = this->_size;
        ft_size_t right_length = other._size;
        ft_size_t result_length = left_length + right_length;

        result.reserve(result_length + 1);
        if (ft_big_number::_last_error == FT_ERR_SUCCESS)
        {
            ft_size_t fill_index = 0;

            while (fill_index < result_length)
            {
                result._digits[fill_index] = '0';
                fill_index++;
            }
            result._digits[result_length] = '\0';
            result._size = result_length;
            ft_size_t left_offset = 0;

            while (left_offset < left_length
                    && ft_big_number::_last_error == FT_ERR_SUCCESS)
            {
                ft_size_t left_index = left_length - 1 - left_offset;
                int32_t left_digit_value = this->_digits[left_index] - '0';
                int32_t carry_value = 0;
                ft_size_t right_offset = 0;

                while (right_offset < right_length)
                {
                    ft_size_t right_index = right_length - 1 - right_offset;
                    int32_t right_digit_value = other._digits[right_index] - '0';
                    ft_size_t result_position = result_length - 1
                        - (left_offset + right_offset);
                    int32_t existing_value = result._digits[result_position] - '0';
                    int32_t total_value = left_digit_value * right_digit_value
                        + existing_value + carry_value;

                    result._digits[result_position] =
                        static_cast<char>('0' + (total_value % 10));
                    carry_value = total_value / 10;
                    right_offset++;
                }
                ft_size_t carry_position = result_length - 1
                    - (left_offset + right_length);

                while (carry_value > 0)
                {
                    int32_t existing_value = result._digits[carry_position] - '0';
                    int32_t total_value = existing_value + carry_value;

                    result._digits[carry_position] = static_cast<char>('0'
                            + (total_value % 10));
                    carry_value = total_value / 10;
                    if (carry_position == 0)
                        break ;
                    carry_position--;
                }
                left_offset++;
            }
            if (ft_big_number::_last_error == FT_ERR_SUCCESS)
            {
                result.trim_leading_zeros();
                if (ft_big_number::_last_error == FT_ERR_SUCCESS
                        && this->_is_negative != other._is_negative)
                    result._is_negative = FT_TRUE;
                if (ft_big_number::_last_error == FT_ERR_SUCCESS
                        && result.is_zero_value())
                    result._is_negative = FT_FALSE;
            }
        }
    }
finalize_multiply:
    if (operation_error == FT_ERR_SUCCESS
            && ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
    }
    {
        (void)ft_big_number::unlock_pair(lower, upper);
        if (operation_error == FT_ERR_SUCCESS)
        {
            ft_big_number::set_error(FT_ERR_SUCCESS);
        }
    }
    return (result);
}

ft_big_number_proxy ft_big_number::operator/(const ft_big_number& other) const noexcept
{
    ft_big_number result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t lock_error;
    int32_t operation_error;
    ft_big_number remainder;
    int32_t result_initialization_error;
    int32_t remainder_initialization_error;
    ft_size_t digit_index;
    int32_t magnitude_comparison;
    int32_t best_digit_value;
    int32_t candidate_digit;

    operation_error = FT_ERR_SUCCESS;
    result_initialization_error = result.initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = result_initialization_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_division;
    }
    remainder_initialization_error = remainder.initialize();
    if (remainder_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = remainder_initialization_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_division;
    }
    digit_index = 0;
    magnitude_comparison = 0;
    lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        operation_error = lock_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_division;
    }
    if (ft_big_number::_last_error != 0)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_division;
    }
    if (other.is_zero_value())
    {
        operation_error = FT_ERR_DIVIDE_BY_ZERO;
        ft_big_number::set_error(operation_error);
        goto cleanup_division;
    }
    if (this->is_zero_value())
        goto cleanup_division;
    magnitude_comparison = this->compare_magnitude(other);
    if (magnitude_comparison < 0)
        goto cleanup_division;
    if (magnitude_comparison == 0)
    {
        result.append_digit('1');
        if (ft_big_number::_last_error == FT_ERR_SUCCESS
                && this->_is_negative != other._is_negative)
            result._is_negative = FT_TRUE;
        if (ft_big_number::_last_error == FT_ERR_SUCCESS
                && result.is_zero_value())
            result._is_negative = FT_FALSE;
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
            operation_error = ft_big_number::_last_error;
        goto cleanup_division;
    }
    while (digit_index < this->_size)
    {
        remainder.append_digit(this->_digits[digit_index]);
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            operation_error = ft_big_number::_last_error;
            goto cleanup_division;
        }
        remainder.trim_leading_zeros();
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            operation_error = ft_big_number::_last_error;
            goto cleanup_division;
        }
        best_digit_value = 0;
        if (remainder.compare_magnitude(other) >= 0)
        {
            ft_big_number best_product;
            int32_t best_product_initialization_error;

            best_product_initialization_error = best_product.initialize();
            if (best_product_initialization_error != FT_ERR_SUCCESS)
            {
                operation_error = best_product_initialization_error;
                ft_big_number::set_error(operation_error);
                goto cleanup_division;
            }

            candidate_digit = 1;
            while (candidate_digit <= 9)
            {
                ft_big_number digit_multiplier;
                int32_t digit_multiplier_initialization_error;

                digit_multiplier_initialization_error = digit_multiplier.initialize();
                if (digit_multiplier_initialization_error != FT_ERR_SUCCESS)
                {
                    operation_error = digit_multiplier_initialization_error;
                    ft_big_number::set_error(operation_error);
                    goto cleanup_division;
                }

                digit_multiplier.append_digit(static_cast<char>('0' + candidate_digit));
                if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                {
                    operation_error = ft_big_number::_last_error;
                    goto cleanup_division;
                }
                ft_big_number candidate_product;
                int32_t candidate_product_initialization_error;

                candidate_product_initialization_error = candidate_product.initialize();
                if (candidate_product_initialization_error != FT_ERR_SUCCESS)
                {
                    operation_error = candidate_product_initialization_error;
                    ft_big_number::set_error(operation_error);
                    goto cleanup_division;
                }
                candidate_product = other * digit_multiplier;

                if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                {
                    operation_error = ft_big_number::_last_error;
                    goto cleanup_division;
                }
                int32_t product_comparison = candidate_product.compare_magnitude(remainder);

                if (product_comparison <= 0)
                {
                    best_digit_value = candidate_digit;
                    best_product = candidate_product;
                    if (product_comparison == 0)
                        candidate_digit = 10;
                    else
                        candidate_digit++;
                }
                else
                    candidate_digit = 10;
            }
            if (best_digit_value > 0)
            {
                ft_big_number *magnitude_result;

                magnitude_result = remainder.subtract_magnitude(best_product);
                if (magnitude_result == ft_nullptr)
                {
                    operation_error = ft_big_number::_last_error;
                    goto cleanup_division;
                }
                remainder = *magnitude_result;
                (void)magnitude_result->destroy();
                delete magnitude_result;
                if (ft_big_number::_last_error != FT_ERR_SUCCESS)
                {
                    operation_error = ft_big_number::_last_error;
                    goto cleanup_division;
                }
            }
        }
        char quotient_digit = static_cast<char>('0' + best_digit_value);

        result.append_digit(quotient_digit);
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            operation_error = ft_big_number::_last_error;
            goto cleanup_division;
        }
        digit_index++;
    }
    result.trim_leading_zeros();
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        operation_error = ft_big_number::_last_error;
    if (operation_error == FT_ERR_SUCCESS)
    {
        if (this->_is_negative != other._is_negative)
            result._is_negative = FT_TRUE;
        if (result.is_zero_value())
            result._is_negative = FT_FALSE;
    }
cleanup_division:
    {
        (void)ft_big_number::unlock_pair(lower, upper);
        if (operation_error == FT_ERR_SUCCESS)
        {
            ft_big_number::set_error(FT_ERR_SUCCESS);
        }
    }
    if (operation_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(operation_error);
        return (ft_big_number_proxy(operation_error));
    }
    return (result);
}

ft_big_number_proxy ft_big_number::operator%(const ft_big_number& other) const noexcept
{
    ft_big_number result;
    ft_big_number quotient;
    ft_big_number product;
    int32_t result_initialization_error;
    int32_t quotient_initialization_error;
    int32_t product_initialization_error;
    int32_t operation_error;

    operation_error = FT_ERR_SUCCESS;
    result_initialization_error = result.initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = result_initialization_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    quotient_initialization_error = quotient.initialize();
    if (quotient_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = quotient_initialization_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    product_initialization_error = product.initialize();
    if (product_initialization_error != FT_ERR_SUCCESS)
    {
        operation_error = product_initialization_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    quotient = this->operator/(other);

    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    product = quotient * other;

    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    result = this->operator-(product);
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        ft_big_number::set_error(operation_error);
        goto cleanup_modulus;
    }
    result.trim_leading_zeros();
    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        operation_error = ft_big_number::_last_error;
        goto cleanup_modulus;
    }
    if (result.is_zero_value())
        result._is_negative = FT_FALSE;
cleanup_modulus:
    if (operation_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(operation_error);
        return (ft_big_number_proxy(operation_error));
    }
    return (result);
}

ft_bool ft_big_number::operator!=(const ft_big_number& other) const noexcept
{
    if (this->operator==(other))
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool ft_big_number::operator<(const ft_big_number& other) const noexcept
{
    ft_bool result;
    int32_t operation_error;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;

    operation_error = FT_ERR_SUCCESS;
    {
        int32_t lock_error = ft_big_number::lock_pair(*this, other, lower, upper);

        if (lock_error != FT_ERR_SUCCESS)
        {
            operation_error = lock_error;
            result = FT_FALSE;
        }
        else if (ft_big_number::_last_error != 0)
        {
            operation_error = ft_big_number::_last_error;
            result = FT_FALSE;
        }
        else if (this->is_zero_value() && other.is_zero_value())
            result = FT_FALSE;
        else if (this->_is_negative && !other._is_negative)
            result = FT_TRUE;
        else if (!this->_is_negative && other._is_negative)
            result = FT_FALSE;
        else
        {
            int32_t magnitude_comparison = this->compare_magnitude(other);

            if (this->_is_negative && other._is_negative)
                result = (magnitude_comparison > 0);
            else
                result = (magnitude_comparison < 0);
        }
    }
    {
        (void)ft_big_number::unlock_pair(lower, upper);
    }
    if (operation_error != FT_ERR_SUCCESS)
        ft_big_number::set_error(operation_error);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_bool ft_big_number::operator<=(const ft_big_number& other) const noexcept
{
    if (this->operator<(other))
        return (FT_TRUE);
    return (this->operator==(other));
}

ft_bool ft_big_number::operator>(const ft_big_number& other) const noexcept
{
    if (other.operator<(*this))
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool ft_big_number::operator>=(const ft_big_number& other) const noexcept
{
    if (this->operator>(other))
        return (FT_TRUE);
    return (this->operator==(other));
}

ft_bool ft_big_number::operator==(const ft_big_number& other) const noexcept
{
    ft_bool are_equal;
    int32_t operation_error;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;

    operation_error = FT_ERR_SUCCESS;
    {
        int32_t lock_error = ft_big_number::lock_pair(*this, other, lower, upper);

        if (lock_error != FT_ERR_SUCCESS)
        {
            operation_error = lock_error;
            are_equal = FT_FALSE;
        }
        else if (ft_big_number::_last_error != 0)
        {
            operation_error = ft_big_number::_last_error;
            are_equal = FT_FALSE;
        }
        else if (this->is_zero_value() && other.is_zero_value())
            are_equal = FT_TRUE;
        else if (this->_is_negative != other._is_negative)
            are_equal = FT_FALSE;
        else if (this->_size != other._size)
            are_equal = FT_FALSE;
        else if (this->_size == 0)
            are_equal = FT_TRUE;
        else if (!this->_digits || !other._digits)
            are_equal = FT_FALSE;
        else
        {
            ft_size_t digit_index = 0;

            are_equal = FT_TRUE;
            while (digit_index < this->_size)
            {
                if (this->_digits[digit_index] != other._digits[digit_index])
                {
                    are_equal = FT_FALSE;
                    break ;
                }
                digit_index++;
            }
        }
    }
    {
        (void)ft_big_number::unlock_pair(lower, upper);
    }
    if (operation_error != FT_ERR_SUCCESS)
        ft_big_number::set_error(operation_error);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (are_equal);
}

const char* ft_big_number::c_str() const noexcept
{
    const char* digits_pointer;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ("0");
    }
    if (this->_digits && this->_size > 0)
        digits_pointer = this->_digits;
    else
        digits_pointer = "0";
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (digits_pointer);
}

ft_size_t ft_big_number::size() const noexcept
{
    ft_size_t current_size;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (0);
    }
    current_size = this->_size;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (current_size);
}

ft_bool ft_big_number::empty() const noexcept
{
    ft_bool is_empty_result;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty_result = (this->_size == 0);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (is_empty_result);
}

ft_bool ft_big_number::is_negative() const noexcept
{
    ft_bool result;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (FT_FALSE);
    }
    if (this->is_zero_value())
        result = FT_FALSE;
    else
        result = this->_is_negative;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_bool ft_big_number::is_positive() const noexcept
{
    ft_bool result;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (FT_FALSE);
    }
    if (this->is_zero_value())
        result = FT_FALSE;
    else
        result = !this->_is_negative;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_string *ft_big_number::to_string_base(int32_t base) noexcept
{
    if (base < 2 || base > 16)
    {
        ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_string::from_error(FT_ERR_INVALID_ARGUMENT));
    }
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);

    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (ft_string::from_error(lock_error));
    }
    auto finalize_to_string = [&](ft_string *result_value, int32_t error_code) noexcept -> ft_string *
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        ft_big_number::set_error(error_code);
        return (result_value);
    };
    if (ft_big_number::_last_error != 0)
    {
        int32_t stored_error = ft_big_number::_last_error;

        return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
    }
    ft_bool is_zero_value_result = this->is_zero_value();
    ft_bool original_negative = this->_is_negative;

    if (base == 10 || is_zero_value_result)
    {
        const char *magnitude_text = "0";
        ft_size_t digits = 1U;
        ft_size_t sign = 0U;
        if (!is_zero_value_result)
        {
            ft_size_t first_digit = 0U;
            while (this->_digits[first_digit] == '0')
                ++first_digit;
            magnitude_text = this->_digits + first_digit;
            digits = this->_size - first_digit;
            sign = original_negative != FT_FALSE;
        }
        if (digits > FT_SYSTEM_SIZE_MAX - sign - 1U)
            return (finalize_to_string(ft_string::from_error(FT_ERR_OUT_OF_RANGE),
                FT_ERR_OUT_OF_RANGE));
        const ft_size_t length = digits + sign;
        char *decimal = static_cast<char *>(cma_malloc(length + 1U));
        if (decimal == ft_nullptr)
            return (finalize_to_string(ft_string::from_error(FT_ERR_NO_MEMORY), FT_ERR_NO_MEMORY));
        if (sign != 0U)
            decimal[0] = '-';
        ft_memcpy(decimal + sign, magnitude_text, digits);
        decimal[length] = '\0';
        ft_string *result = new (std::nothrow) ft_string();
        if (result == ft_nullptr)
        {
            cma_free(decimal);
            return (finalize_to_string(ft_nullptr, FT_ERR_NO_MEMORY));
        }
        const int32_t error = ft_string_take_owned_buffer(*result, decimal, length);
        if (error != FT_ERR_SUCCESS)
        {
            cma_free(decimal);
            delete result;
            return (finalize_to_string(ft_string::from_error(error), error));
        }
        return (finalize_to_string(result, FT_ERR_SUCCESS));
    }
    ft_big_number magnitude;
    int32_t magnitude_initialization_error = magnitude.initialize(*this);

    if (magnitude_initialization_error != FT_ERR_SUCCESS)
        return (finalize_to_string(ft_string::from_error(magnitude_initialization_error),
                    magnitude_initialization_error));
    magnitude._is_negative = FT_FALSE;
    ft_big_number base_value;
    int32_t base_value_initialization_error;

    base_value_initialization_error = base_value.initialize();
    if (base_value_initialization_error != FT_ERR_SUCCESS)
        return (finalize_to_string(ft_string::from_error(base_value_initialization_error),
                    base_value_initialization_error));

    base_value.append_unsigned(static_cast<uint64_t>(base));
    if (ft_big_number::_last_error != 0)
    {
        int32_t stored_error = ft_big_number::_last_error;

        return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
    }
    ft_string digits;
    int32_t digits_initialization_error;

    digits_initialization_error = digits.initialize();
    if (digits_initialization_error != FT_ERR_SUCCESS)
        return (finalize_to_string(ft_string::from_error(digits_initialization_error),
                    digits_initialization_error));

    while (!magnitude.is_zero_value())
    {
        ft_big_number quotient;
        int32_t quotient_initialization_error;

        quotient_initialization_error = quotient.initialize();
        if (quotient_initialization_error != FT_ERR_SUCCESS)
            return (finalize_to_string(ft_string::from_error(quotient_initialization_error),
                        quotient_initialization_error));
        quotient = magnitude / base_value;
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            int32_t stored_error = ft_big_number::_last_error;

            return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
        }
        ft_big_number product;
        int32_t product_initialization_error;

        product_initialization_error = product.initialize();
        if (product_initialization_error != FT_ERR_SUCCESS)
            return (finalize_to_string(ft_string::from_error(product_initialization_error),
                        product_initialization_error));
        product = quotient * base_value;
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            int32_t stored_error = ft_big_number::_last_error;

            return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
        }
        ft_big_number remainder_number;
        int32_t remainder_number_initialization_error;

        remainder_number_initialization_error = remainder_number.initialize();
        if (remainder_number_initialization_error != FT_ERR_SUCCESS)
            return (finalize_to_string(ft_string::from_error(remainder_number_initialization_error),
                        remainder_number_initialization_error));
        remainder_number = magnitude - product;
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            int32_t stored_error = ft_big_number::_last_error;

            return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
        }
        const char* remainder_digits = remainder_number.c_str();

        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            int32_t stored_error = ft_big_number::_last_error;

            return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
        }
        ft_size_t remainder_index = 0;
        int32_t remainder_value = 0;

        while (remainder_digits[remainder_index] != '\0')
        {
            remainder_value = remainder_value * 10 + (remainder_digits[remainder_index] - '0');
            remainder_index++;
        }
        char digit_symbol = ft_big_number_digit_symbol(remainder_value);

        if (digit_symbol == '\0')
            return (finalize_to_string(ft_string::from_error(FT_ERR_INVALID_ARGUMENT),
                        FT_ERR_INVALID_ARGUMENT));
        digits.append(digit_symbol);
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
            return (finalize_to_string(ft_string::from_error(FT_ERR_NO_MEMORY),
                        FT_ERR_NO_MEMORY));
        magnitude = quotient;
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            int32_t stored_error = ft_big_number::_last_error;

            return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
        }
        magnitude._is_negative = FT_FALSE;
    }
    ft_string *result;
    int32_t result_initialization_error;

    result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
        return (finalize_to_string(ft_nullptr, FT_ERR_NO_MEMORY));
    result_initialization_error = result->initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        return (finalize_to_string(ft_string::from_error(result_initialization_error),
                    result_initialization_error));
    }

    if (original_negative)
    {
        result->append('-');
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            (void)result->destroy();
            delete result;
            return (finalize_to_string(ft_string::from_error(FT_ERR_NO_MEMORY),
                        FT_ERR_NO_MEMORY));
        }
    }
    const char* digits_buffer = digits.c_str();
    ft_size_t digits_length = digits.size();

    if (ft_big_number::_last_error != FT_ERR_SUCCESS)
    {
        int32_t stored_error = ft_big_number::_last_error;

        return (finalize_to_string(ft_string::from_error(stored_error), stored_error));
    }
    while (digits_length > 0)
    {
        digits_length--;
        result->append(digits_buffer[digits_length]);
        if (ft_big_number::_last_error != FT_ERR_SUCCESS)
        {
            (void)result->destroy();
            delete result;
            return (finalize_to_string(ft_string::from_error(FT_ERR_NO_MEMORY),
                        FT_ERR_NO_MEMORY));
        }
    }
    return (finalize_to_string(result, FT_ERR_SUCCESS));
}

ft_big_number *ft_big_number::mod_pow(const ft_big_number& exponent, const ft_big_number& modulus) const noexcept
{
    const auto make_error_result = [&](int32_t error_code) -> ft_big_number *
    {
        ft_big_number *error_result;
        int32_t error_result_initialization_error;
        int32_t stored_error_code;

        error_result = new (std::nothrow) ft_big_number();
        stored_error_code = error_code;
        if (stored_error_code == FT_ERR_SUCCESS)
            stored_error_code = FT_ERR_INVALID_STATE;
        if (error_result == ft_nullptr)
        {
            ft_big_number::set_error(FT_ERR_NO_MEMORY);
            return (ft_nullptr);
        }
        error_result_initialization_error = error_result->initialize();
        if (error_result_initialization_error != FT_ERR_SUCCESS)
        {
            delete error_result;
            ft_big_number::set_error(stored_error_code);
            return (ft_nullptr);
        }
        error_result->_operation_error = stored_error_code;
        ft_big_number::set_error(stored_error_code);
        return (error_result);
    };

    ft_big_number base_value;
    int32_t base_value_initialization_error = base_value.initialize(*this);

    if (base_value_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(base_value_initialization_error));
    ft_big_number exponent_value;
    int32_t exponent_value_initialization_error = exponent_value.initialize(exponent);

    if (exponent_value_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(exponent_value_initialization_error));
    ft_big_number modulus_value;
    int32_t modulus_value_initialization_error = modulus_value.initialize(modulus);

    if (modulus_value_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(modulus_value_initialization_error));
    if (modulus_value.is_zero_value())
        return (make_error_result(FT_ERR_DIVIDE_BY_ZERO));
    if (modulus_value.is_negative())
        return (make_error_result(FT_ERR_INVALID_ARGUMENT));
    if (exponent_value.is_negative())
        return (make_error_result(FT_ERR_INVALID_ARGUMENT));
    modulus_value._is_negative = FT_FALSE;
    modulus_value.trim_leading_zeros();
    ft_big_number base_remainder;
    int32_t base_remainder_initialization_error;

    base_remainder_initialization_error = base_remainder.initialize();
    if (base_remainder_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(base_remainder_initialization_error));
    base_remainder = base_value % modulus_value;
    if (ft_big_number::_last_error != 0)
        return (make_error_result(ft_big_number::_last_error));
    while (base_remainder.is_negative())
    {
        ft_big_number adjusted_base;
        int32_t adjusted_base_initialization_error;

        adjusted_base_initialization_error = adjusted_base.initialize();
        if (adjusted_base_initialization_error != FT_ERR_SUCCESS)
            return (make_error_result(adjusted_base_initialization_error));
        adjusted_base = base_remainder + modulus_value;
        if (ft_big_number::_last_error != 0)
            return (make_error_result(ft_big_number::_last_error));
        base_remainder = adjusted_base;
    }
    base_remainder.trim_leading_zeros();
    exponent_value._is_negative = FT_FALSE;
    exponent_value.trim_leading_zeros();
    ft_big_number one;
    int32_t one_initialization_error;

    one_initialization_error = one.initialize();
    if (one_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(one_initialization_error));
    one.append_digit('1');
    if (ft_big_number::_last_error != 0)
        return (make_error_result(ft_big_number::_last_error));
    if (exponent_value.is_zero_value())
    {
        ft_big_number *identity_result;
        int32_t identity_result_initialization_error;

        identity_result = new (std::nothrow) ft_big_number();
        if (identity_result == ft_nullptr)
            return (make_error_result(FT_ERR_NO_MEMORY));
        identity_result_initialization_error = identity_result->initialize();
        if (identity_result_initialization_error != FT_ERR_SUCCESS)
        {
            delete identity_result;
            return (make_error_result(identity_result_initialization_error));
        }
        *identity_result = one % modulus_value;
        if (ft_big_number::_last_error != 0)
        {
            (void)identity_result->destroy();
            delete identity_result;
            return (make_error_result(ft_big_number::_last_error));
        }
        identity_result->_is_negative = FT_FALSE;
        if (identity_result->is_zero_value())
            identity_result->_is_negative = FT_FALSE;
        identity_result->trim_leading_zeros();
        return (identity_result);
    }
    ft_big_number accumulator;
    int32_t accumulator_initialization_error = accumulator.initialize(one);

    if (accumulator_initialization_error != FT_ERR_SUCCESS)
        return (make_error_result(accumulator_initialization_error));
    while (!exponent_value.is_zero_value())
    {
        ft_big_number product_value;
        int32_t product_value_initialization_error;

        product_value_initialization_error = product_value.initialize();
        if (product_value_initialization_error != FT_ERR_SUCCESS)
            return (make_error_result(product_value_initialization_error));
        product_value = accumulator * base_remainder;
        if (ft_big_number::_last_error != 0)
            return (make_error_result(ft_big_number::_last_error));
        ft_big_number reduced_value;
        int32_t reduced_value_initialization_error;

        reduced_value_initialization_error = reduced_value.initialize();
        if (reduced_value_initialization_error != FT_ERR_SUCCESS)
            return (make_error_result(reduced_value_initialization_error));
        reduced_value = product_value % modulus_value;
        if (ft_big_number::_last_error != 0)
            return (make_error_result(ft_big_number::_last_error));
        accumulator = reduced_value;
        ft_big_number next_exponent;
        int32_t next_exponent_initialization_error;

        next_exponent_initialization_error = next_exponent.initialize();
        if (next_exponent_initialization_error != FT_ERR_SUCCESS)
            return (make_error_result(next_exponent_initialization_error));
        next_exponent = exponent_value - one;
        if (ft_big_number::_last_error != 0)
            return (make_error_result(ft_big_number::_last_error));
        exponent_value = next_exponent;
        exponent_value._is_negative = FT_FALSE;
        exponent_value.trim_leading_zeros();
    }
    accumulator._is_negative = FT_FALSE;
    if (accumulator.is_zero_value())
        accumulator._is_negative = FT_FALSE;
    accumulator.trim_leading_zeros();
    ft_big_number *result;
    int32_t result_initialization_error;

    result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
        return (make_error_result(FT_ERR_NO_MEMORY));
    result_initialization_error = result->initialize(accumulator);
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        return (make_error_result(result_initialization_error));
    }
    return (result);
}

ft_big_number_proxy::ft_big_number_proxy() noexcept
    : _value(), _last_error(FT_ERR_SUCCESS)
{
    return ;
}

ft_big_number_proxy::ft_big_number_proxy(int32_t error_code) noexcept
        : _value(), _last_error(error_code)
{
    return ;
}

ft_big_number_proxy::ft_big_number_proxy(const ft_big_number &value) noexcept
    : _value(), _last_error(FT_ERR_SUCCESS)
{
    int32_t initialization_error = this->_value.initialize(value);
    if (initialization_error != FT_ERR_SUCCESS)
        this->_last_error = initialization_error;
    return ;
}

ft_big_number_proxy::ft_big_number_proxy(const ft_big_number &value, int32_t error_code) noexcept
    : _value(), _last_error(error_code)
{
    int32_t initialization_error = this->_value.initialize(value);
    if (initialization_error != FT_ERR_SUCCESS)
        this->_last_error = initialization_error;
    return ;
}

ft_big_number_proxy::ft_big_number_proxy(const ft_big_number_proxy &other) noexcept
    : _value(), _last_error(other._last_error)
{
    int32_t initialization_error;

    if (other._value._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialization_error = this->_value.initialize(other._value);
        if (initialization_error != FT_ERR_SUCCESS)
            this->_last_error = initialization_error;
    }
    return ;
}

ft_big_number_proxy::ft_big_number_proxy(ft_big_number_proxy &&other) noexcept
    : _value(), _last_error(other._last_error)
{
    int32_t initialization_error;

    if (other._value._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialization_error = this->_value.initialize(ft_move(other._value));
        if (initialization_error != FT_ERR_SUCCESS)
            this->_last_error = initialization_error;
    }
    other._last_error = FT_ERR_SUCCESS;
    return ;
}

ft_big_number_proxy::~ft_big_number_proxy()
{
    return ;
}

ft_big_number_proxy ft_big_number_proxy::operator+(const ft_big_number &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(this->_last_error);
        return (ft_big_number_proxy(this->_last_error));
    }
    return (this->_value + right);
}

ft_big_number_proxy ft_big_number_proxy::operator-(const ft_big_number &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(this->_last_error);
        return (ft_big_number_proxy(this->_last_error));
    }
    return (this->_value - right);
}

ft_big_number_proxy ft_big_number_proxy::operator*(const ft_big_number &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(this->_last_error);
        return (ft_big_number_proxy(this->_last_error));
    }
    return (this->_value * right);
}

ft_big_number_proxy ft_big_number_proxy::operator/(const ft_big_number &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(this->_last_error);
        return (ft_big_number_proxy(this->_last_error));
    }
    return (this->_value / right);
}

ft_big_number_proxy ft_big_number_proxy::operator%(const ft_big_number &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(this->_last_error);
        return (ft_big_number_proxy(this->_last_error));
    }
    return (this->_value % right);
}

ft_big_number_proxy::operator ft_big_number *() const noexcept
{
    ft_big_number *result;
    int32_t initialization_error;

    result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (this->_last_error != FT_ERR_SUCCESS)
    {
        initialization_error = result->initialize();
        if (initialization_error != FT_ERR_SUCCESS)
        {
            delete result;
            ft_big_number::set_error(initialization_error);
            return (ft_nullptr);
        }
        ft_big_number::set_error(this->_last_error);
        return (result);
    }
    if (this->_value._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        initialization_error = result->initialize();
        if (initialization_error != FT_ERR_SUCCESS)
        {
            delete result;
            ft_big_number::set_error(initialization_error);
            return (ft_nullptr);
        }
        ft_big_number::set_error(FT_ERR_INVALID_STATE);
        return (result);
    }
    initialization_error = result->initialize(this->_value);
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(initialization_error);
        return (ft_nullptr);
    }
    ft_big_number::set_error(this->_last_error);
    return (result);
}

int32_t ft_big_number_proxy::get_error() const noexcept
{
    return (this->_last_error);
}

ft_big_number_proxy operator+(const ft_big_number_proxy &left, const ft_big_number &right) noexcept
{
    return (left.operator+(right));
}

ft_big_number_proxy operator-(const ft_big_number_proxy &left, const ft_big_number &right) noexcept
{
    return (left.operator-(right));
}

ft_big_number_proxy operator*(const ft_big_number_proxy &left, const ft_big_number &right) noexcept
{
    return (left.operator*(right));
}

ft_big_number_proxy operator/(const ft_big_number_proxy &left, const ft_big_number &right) noexcept
{
    return (left.operator/(right));
}

ft_big_number_proxy operator%(const ft_big_number_proxy &left, const ft_big_number &right) noexcept
{
    return (left.operator%(right));
}
