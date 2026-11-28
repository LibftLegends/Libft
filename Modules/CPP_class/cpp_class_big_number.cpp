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

static constexpr char FT_BIG_NUMBER_DIGITS[] = "0123456789ABCDEF";

static int32_t ft_big_number_digit_value(char digit) noexcept
{
    if (digit >= '0' && digit <= '9')
        return (digit - '0');
    if (digit >= 'A' && digit <= 'F')
        return (digit - 'A' + 10);
    if (digit >= 'a' && digit <= 'f')
        return (digit - 'a' + 10);
    return (-1);
}

static constexpr uint32_t FT_BIG_NUMBER_GROUP_DIGITS = 4U;
static constexpr uint32_t FT_BIG_NUMBER_GROUP_BASE = 10000U;

static ft_size_t ft_big_number_group_count(ft_size_t size) noexcept
{
    return (size / FT_BIG_NUMBER_GROUP_DIGITS
        + (size % FT_BIG_NUMBER_GROUP_DIGITS != 0U));
}

static void ft_big_number_read_groups(const char *digits, ft_size_t size,
    uint32_t *groups) noexcept
{
    while (size != 0U)
    {
        ft_size_t first = 0U;
        if (size > FT_BIG_NUMBER_GROUP_DIGITS)
            first = size - FT_BIG_NUMBER_GROUP_DIGITS;
        uint32_t value = 0U;
        while (first < size)
        {
            value = value * 10U + static_cast<uint32_t>(digits[first] - '0');
            first += 1U;
        }
        *groups = value;
        groups += 1;
        if (size <= FT_BIG_NUMBER_GROUP_DIGITS)
            break ;
        size -= FT_BIG_NUMBER_GROUP_DIGITS;
    }
    return ;
}

static ft_size_t ft_big_number_write_groups(char *digits, const uint32_t *groups,
    ft_size_t count) noexcept
{
    while (count > 1U && groups[count - 1U] == 0U)
        count -= 1U;
    ft_size_t length = 0U;
    uint32_t place = FT_BIG_NUMBER_GROUP_BASE / 10U;
    while (place > 1U && groups[count - 1U] < place)
        place /= 10U;
    while (count != 0U)
    {
        count -= 1U;
        uint32_t value = groups[count];
        while (place != 0U)
        {
            digits[length] = static_cast<char>('0' + value / place);
            length += 1U;
            value %= place;
            place /= 10U;
        }
        place = FT_BIG_NUMBER_GROUP_BASE / 10U;
    }
    digits[length] = '\0';
    return (length);
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

ft_size_t ft_big_number::append_position() const noexcept
{
    if (this->_size == 1U && this->_digits[0] == '0')
        return (0U);
    return (this->_size);
}

int32_t ft_big_number::reserve_append(ft_size_t position, ft_size_t count) noexcept
{
    if (position >= SIZE_MAX || count > SIZE_MAX - position - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t required = position + count + 1U;
    if (required <= this->_capacity)
        return (FT_ERR_SUCCESS);
    if (this->_capacity <= SIZE_MAX / 2U && this->_capacity * 2U > required)
        this->reserve(this->_capacity * 2U);
    if (this->_capacity < required)
        this->reserve(required);
    if (this->_capacity < required)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

void ft_big_number::store_appended_digit(ft_size_t position, char digit) noexcept
{
    this->_digits[position] = digit;
    this->_size = position + 1U;
    this->_digits[this->_size] = '\0';
    return ;
}

int32_t ft_big_number::append_digit_unlocked(char digit) noexcept
{
    if (digit < '0' || digit > '9')
        return (FT_ERR_SUCCESS);
    const ft_size_t position = this->append_position();
    const int32_t error_code = this->reserve_append(position, 1U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->store_appended_digit(position, digit);
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::append_unlocked(const char *digits) noexcept
{
    if (digits == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t position = this->append_position();
    if (position == 0U)
    {
        ft_bool saw_zero = FT_FALSE;
        while (*digits != '\0' && (*digits < '1' || *digits > '9'))
        {
            if (*digits == '0')
                saw_zero = FT_TRUE;
            digits += 1;
        }
        if (*digits == '\0')
        {
            if (saw_zero == FT_TRUE)
                return (this->append_digit_unlocked('0'));
            return (FT_ERR_SUCCESS);
        }
    }

    const char *end = digits;
    while (*end >= '0' && *end <= '9')
        end += 1;
    ft_size_t digit_count = static_cast<ft_size_t>(end - digits);
    while (*end != '\0')
    {
        digit_count += *end >= '0' && *end <= '9';
        end += 1;
    }
    if (digit_count == 0U)
        return (FT_ERR_SUCCESS);
    const ft_size_t length = static_cast<ft_size_t>(end - digits);
    const uintptr_t source_address = reinterpret_cast<uintptr_t>(digits);
    const uintptr_t digits_address = reinterpret_cast<uintptr_t>(this->_digits);
    const ft_bool borrowed = this->_digits != ft_nullptr && source_address >= digits_address
        && source_address - digits_address < this->_size;
    const int32_t error_code = this->reserve_append(position, digit_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (borrowed == FT_TRUE)
        digits = this->_digits + (source_address - digits_address);

    char *output = this->_digits + position;
    if (digit_count == length)
        ft_memcpy(output, digits, length);
    else
    {
        ft_size_t index = 0U;
        while (index < length)
        {
            const char digit = digits[index];
            if (digit >= '0' && digit <= '9')
            {
                *output = digit;
                output += 1;
            }
            index += 1U;
        }
    }
    this->_size = position + digit_count;
    this->_digits[this->_size] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::append_unsigned_unlocked(uint64_t value) noexcept
{
    char digits[21];
    char *first = digits + sizeof(digits) - 1U;
    *first = '\0';
    if (value == 0U)
        return (this->append_digit_unlocked('0'));
    while (value != 0U)
    {
        first -= 1;
        *first = static_cast<char>('0' + value % 10U);
        value /= 10U;
    }
    return (this->append_unlocked(first));
}

void ft_big_number::trim_leading_zeros_unlocked(ft_bool shrink_storage) noexcept
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
    if (shrink_storage == FT_TRUE)
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
    const int32_t copy_error = this->copy_value(other);
    if (copy_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(copy_error));
    }
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
}

int32_t ft_big_number::initialize(ft_big_number &&other) noexcept
{
    return (this->move(other));
}

int32_t ft_big_number::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }
    int32_t disable_error = FT_ERR_SUCCESS;
    if (this->_mutex != ft_nullptr)
        disable_error = this->disable_thread_safety();
    if (this->_digits != ft_nullptr)
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

int32_t ft_big_number::copy_value(const ft_big_number &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    const int32_t lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (other._size != 0U)
    {
        if (other._size >= SIZE_MAX)
        {
            (void)ft_big_number::unlock_pair(lower, upper);
            return (FT_ERR_OUT_OF_RANGE);
        }
        const ft_size_t required_capacity = other._size + 1U;
        if (this->_capacity < required_capacity)
        {
            ft_size_t capacity = required_capacity;
            if (capacity < 16U)
                capacity = 16U;
            char *digits = static_cast<char *>(cma_malloc(capacity));
            if (digits == ft_nullptr)
            {
                (void)ft_big_number::unlock_pair(lower, upper);
                return (FT_ERR_NO_MEMORY);
            }
            cma_free(this->_digits);
            this->_digits = digits;
            this->_capacity = capacity;
        }
        ft_memcpy(this->_digits, other._digits, required_capacity);
    }
    else if (this->_digits != ft_nullptr)
        this->_digits[0] = '\0';
    this->_size = other._size;
    this->_is_negative = other._is_negative;
    (void)ft_big_number::unlock_pair(lower, upper);
    return (FT_ERR_SUCCESS);
}

ft_big_number& ft_big_number::operator=(const ft_big_number &other) noexcept
{
    ft_big_number::set_error(this->copy_value(other));
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
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        ft_big_number::_last_initialised_state = this->_initialised_state;
    const int32_t proxy_error = other.get_error();
    if (proxy_error == FT_ERR_SUCCESS)
        return (*this = other._value);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (*this);
    }
    this->_size = 0U;
    this->_is_negative = FT_FALSE;
    if (this->_digits != ft_nullptr)
        this->_digits[0] = '\0';
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(proxy_error);
    return (*this);
}

ft_big_number& ft_big_number::operator=(ft_big_number_proxy &&other) noexcept
{
    ft_big_number &value = other._value;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_mutex != ft_nullptr || value._mutex != ft_nullptr
        || other.get_error() != FT_ERR_SUCCESS || value._size < this->_capacity
        || value._size == 0U)
        return (*this = static_cast<const ft_big_number_proxy &>(other));
    if (this->_digits != ft_nullptr)
        cma_free(this->_digits);
    this->_digits = value._digits;
    this->_size = value._size;
    this->_capacity = value._capacity;
    this->_is_negative = value._is_negative;
    value._digits = ft_nullptr;
    value._size = 0U;
    value._capacity = 0U;
    value._is_negative = FT_FALSE;
    value._initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_big_number::_last_initialised_state = this->_initialised_state;
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (*this);
}

int32_t ft_big_number::move(ft_big_number &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_big_number::move source",
            "called with uninitialised source object");
        return (ft_big_number::set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (ft_big_number::set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(FT_ERR_SUCCESS));
    }

    pt_recursive_mutex *source_mutex = other._mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(source_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        ft_big_number::_last_initialised_state = this->_initialised_state;
        return (ft_big_number::set_error(lock_error));
    }

    this->_digits = other._digits;
    this->_size = other._size;
    this->_capacity = other._capacity;
    this->_is_negative = other._is_negative;
    this->_operation_error = other._operation_error;
    this->_mutex = source_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._digits = ft_nullptr;
    other._size = 0U;
    other._capacity = 0U;
    other._is_negative = FT_FALSE;
    other._operation_error = FT_ERR_SUCCESS;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(source_mutex);

    ft_big_number::_last_initialised_state = this->_initialised_state;
    return (ft_big_number::set_error(FT_ERR_SUCCESS));
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

int32_t ft_big_number::compare_magnitude(const ft_big_number &other) const noexcept
{
    if (this->_size == other._size)
        return (ft_memcmp(this->_digits, other._digits, this->_size));
    const ft_bool left_zero = this->is_zero_value();
    const ft_bool right_zero = other.is_zero_value();
    if (left_zero == FT_TRUE && right_zero == FT_TRUE)
        return (0);
    if (left_zero == FT_TRUE)
        return (-1);
    if (right_zero == FT_TRUE)
        return (1);
    if (this->_size < other._size)
        return (-1);
    return (1);
}

int32_t ft_big_number::copy_magnitude(ft_big_number &result) const noexcept
{
    if (this->_size == 0U || this->_digits == ft_nullptr)
        return (FT_ERR_SUCCESS);
    result.reserve(this->_size + 1U);
    if (result._capacity <= this->_size)
        return (FT_ERR_NO_MEMORY);
    ft_memcpy(result._digits, this->_digits, this->_size + 1U);
    result._size = this->_size;
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::add_magnitude(const ft_big_number &other,
    ft_big_number &result) const noexcept
{
    if (this->is_zero_value() == FT_TRUE)
        return (other.copy_magnitude(result));
    if (other.is_zero_value() == FT_TRUE)
        return (this->copy_magnitude(result));
    ft_size_t result_length = this->_size;
    if (result_length < other._size)
        result_length = other._size;
    if (result_length > FT_SYSTEM_SIZE_MAX - 2U)
        return (FT_ERR_OUT_OF_RANGE);
    result_length += 1U;
    result.reserve(result_length + 1U);
    if (result._capacity <= result_length)
        return (FT_ERR_NO_MEMORY);

    ft_size_t left_index = this->_size;
    ft_size_t right_index = other._size;
    ft_size_t write_index = result_length;
    int32_t carry = 0;
    while (left_index > 0U || right_index > 0U || carry != 0)
    {
        int32_t digit_sum = carry;
        if (left_index > 0U)
        {
            left_index -= 1U;
            digit_sum += this->_digits[left_index] - '0';
        }
        if (right_index > 0U)
        {
            right_index -= 1U;
            digit_sum += other._digits[right_index] - '0';
        }
        write_index -= 1U;
        result._digits[write_index] = static_cast<char>('0' + digit_sum % 10);
        carry = digit_sum / 10;
    }
    result._size = result_length - write_index;
    if (write_index != 0U)
        ft_memmove(result._digits, result._digits + write_index, result._size);
    result._digits[result._size] = '\0';
    return (FT_ERR_SUCCESS);
}

static void ft_big_number_subtract_digits(char *result, const char *left,
    ft_size_t left_index, const char *right, ft_size_t right_index) noexcept
{
    int32_t borrow = 0;
    while (left_index > 0U)
    {
        left_index -= 1U;
        int32_t digit = left[left_index] - '0' - borrow;
        if (right_index > 0U)
        {
            right_index -= 1U;
            digit -= right[right_index] - '0';
        }
        borrow = digit < 0;
        result[left_index] = static_cast<char>('0' + digit + borrow * 10);
    }
    return ;
}

int32_t ft_big_number::subtract_magnitude(const ft_big_number &other,
    ft_big_number &result) const noexcept
{
    if (this->compare_magnitude(other) <= 0)
        return (FT_ERR_SUCCESS);
    if (other.is_zero_value() == FT_TRUE)
        return (this->copy_magnitude(result));
    result.reserve(this->_size + 1U);
    if (result._capacity <= this->_size)
        return (FT_ERR_NO_MEMORY);
    result._size = this->_size;
    result._digits[result._size] = '\0';
    ft_big_number_subtract_digits(result._digits, this->_digits, this->_size,
        other._digits, other._size);
    result.trim_leading_zeros_unlocked();
    return (FT_ERR_SUCCESS);
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

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    if (!number)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    parsed_negative = FT_FALSE;
    const char *digits = number;
    if (*digits == '-')
    {
        parsed_negative = FT_TRUE;
        digits += 1;
    }
    while (digits[0] == '0' && digits[1] != '\0')
        digits += 1;
    const char *end = digits;
    while (*end >= '0' && *end <= '9')
        end += 1;
    if (*end != '\0' || (end == digits && parsed_negative == FT_TRUE))
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    const ft_size_t length = static_cast<ft_size_t>(end - digits);
    if (length == 0U)
    {
        this->clear_unlocked();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        ft_big_number::set_error(FT_ERR_SUCCESS);
        return ;
    }
    if (length + 1 > this->_capacity || !this->_digits)
    {
        this->reserve(length + 1);
        if (length + 1 > this->_capacity || !this->_digits)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            ft_big_number::set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    ft_memcpy(this->_digits, digits, length);
    this->_digits[length] = '\0';
    this->_size = length;
    this->_is_negative = parsed_negative == FT_TRUE
        && (length != 1U || this->_digits[0] != '0');
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return ;
}

static ft_size_t ft_big_number_accumulate_base_groups(const char *digits, const char *end,
    uint32_t base, uint32_t *groups) noexcept
{
    groups[0] = 0U;
    ft_size_t count = 1U;
    while (digits != end)
    {
        uint32_t scale = 1U;
        uint32_t carry = 0U;
        uint32_t digit_count = 0U;
        while (digits != end && digit_count < FT_BIG_NUMBER_GROUP_DIGITS)
        {
            carry = carry * base + static_cast<uint32_t>(ft_big_number_digit_value(*digits));
            scale *= base;
            digits += 1;
            digit_count += 1U;
        }
        ft_size_t group_index = 0U;
        while (group_index < count)
        {
            const uint32_t value = groups[group_index] * scale + carry;
            groups[group_index] = value % FT_BIG_NUMBER_GROUP_BASE;
            carry = value / FT_BIG_NUMBER_GROUP_BASE;
            group_index += 1U;
        }
        while (carry != 0U)
        {
            groups[count] = carry % FT_BIG_NUMBER_GROUP_BASE;
            count += 1U;
            carry /= FT_BIG_NUMBER_GROUP_BASE;
        }
    }
    return (count);
}

int32_t ft_big_number::assign_base_magnitude(const char *digits, uint32_t base,
    ft_bool negative) noexcept
{
    if (digits == ft_nullptr || *digits == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    while (*digits == '0')
        digits += 1;
    const char *end = digits;
    while (*end != '\0')
    {
        const int32_t digit = ft_big_number_digit_value(*end);
        if (digit < 0 || static_cast<uint32_t>(digit) >= base)
            return (FT_ERR_INVALID_ARGUMENT);
        end += 1;
    }

    constexpr ft_size_t INLINE_GROUPS = 128U;
    uint32_t inline_groups[INLINE_GROUPS];
    const ft_size_t input_length = static_cast<ft_size_t>(end - digits);
    if (input_length > (SIZE_MAX - FT_BIG_NUMBER_GROUP_DIGITS - 1U) / 2U)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t maximum_decimal_digits = input_length;
    // Four digits in any supported input base need at most five decimal digits.
    if (base > 10U)
        maximum_decimal_digits += ft_big_number_group_count(input_length);
    const ft_size_t maximum_groups = ft_big_number_group_count(maximum_decimal_digits + 1U);
    uint32_t *groups = inline_groups;
    if (maximum_groups > INLINE_GROUPS)
    {
        groups = static_cast<uint32_t *>(cma_malloc(maximum_groups * sizeof(uint32_t)));
        if (groups == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    const ft_size_t group_count = ft_big_number_accumulate_base_groups(digits, end, base, groups);
    uint32_t highest_group = groups[group_count - 1U];
    ft_size_t length = (group_count - 1U) * FT_BIG_NUMBER_GROUP_DIGITS + 1U;
    while (highest_group >= 10U)
    {
        highest_group /= 10U;
        length += 1U;
    }
    this->reserve(length + 1U);
    int32_t error_code = FT_ERR_NO_MEMORY;
    if (this->_capacity > length)
    {
        this->_size = ft_big_number_write_groups(this->_digits, groups, group_count);
        this->_is_negative = negative == FT_TRUE && (length > 1U || groups[0] != 0U);
        error_code = FT_ERR_SUCCESS;
    }
    if (groups != inline_groups)
        cma_free(groups);
    return (error_code);
}

int32_t ft_big_number::assign_base_digits(const char *digits, uint32_t base,
    ft_bool negative) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_big_number::set_error(lock_error));
    const int32_t error_code = this->assign_base_magnitude(digits, base, negative);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (ft_big_number::set_error(error_code));
}

int32_t ft_big_number::assign_base(const char *digits, int32_t base) noexcept
{
    if (digits == ft_nullptr || *digits == '\0' || base < 2 || base > 16)
        return (ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT));
    if (base == 10)
    {
        this->assign(digits);
        return (ft_big_number::_last_error);
    }
    if (*digits == '-')
        return (this->assign_base_digits(digits + 1, static_cast<uint32_t>(base), FT_TRUE));
    return (this->assign_base_digits(digits, static_cast<uint32_t>(base), FT_FALSE));
}

void ft_big_number::append_digit(char digit) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    const int32_t error_code = this->append_digit_unlocked(digit);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(error_code);
    return ;
}

void ft_big_number::append(const char *digits) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    const int32_t error_code = this->append_unlocked(digits);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(error_code);
    return ;
}

void ft_big_number::append_unsigned(uint64_t value) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return ;
    }
    const int32_t error_code = this->append_unsigned_unlocked(value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_big_number::set_error(error_code);
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

ft_big_number_proxy ft_big_number::sum_or_difference(const ft_big_number &other,
    ft_bool subtract) const noexcept
{
    ft_big_number_proxy result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t error_code = result._value.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = ft_big_number::lock_pair(*this, other, lower, upper);
    if (error_code == FT_ERR_SUCCESS)
    {
        ft_bool right_negative = other._is_negative;
        if (subtract == FT_TRUE)
            right_negative = right_negative == FT_FALSE && other.is_zero_value() == FT_FALSE;
        ft_bool result_negative = this->_is_negative;
        if (this->_is_negative == right_negative)
            error_code = this->add_magnitude(other, result._value);
        else
        {
            const int32_t comparison = this->compare_magnitude(other);
            if (comparison > 0)
                error_code = this->subtract_magnitude(other, result._value);
            else if (comparison < 0)
            {
                error_code = other.subtract_magnitude(*this, result._value);
                result_negative = right_negative;
            }
        }
        result._value._is_negative = error_code == FT_ERR_SUCCESS
            && result_negative == FT_TRUE && result._value.is_zero_value() == FT_FALSE;
        (void)ft_big_number::unlock_pair(lower, upper);
    }
    result._last_error = ft_big_number::set_error(error_code);
    return (result);
}

ft_big_number_proxy ft_big_number::operator+(const ft_big_number &other) const noexcept
{
    return (this->sum_or_difference(other, FT_FALSE));
}

ft_big_number_proxy ft_big_number::operator-(const ft_big_number &other) const noexcept
{
    return (this->sum_or_difference(other, FT_TRUE));
}

int32_t ft_big_number::multiply_grouped_magnitude(const ft_big_number &other,
    ft_big_number &result) const noexcept
{
    constexpr ft_size_t INLINE_GROUPS = 128U;
    uint32_t inline_groups[INLINE_GROUPS];
    const ft_size_t left_count = ft_big_number_group_count(this->_size);
    const ft_size_t right_count = ft_big_number_group_count(other._size);
    const ft_size_t product_count = left_count + right_count;
    if (product_count > SIZE_MAX / (2U * sizeof(uint32_t)))
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t workspace_count = product_count * 2U;
    uint32_t *groups = inline_groups;
    if (workspace_count > INLINE_GROUPS)
    {
        groups = static_cast<uint32_t *>(cma_malloc(workspace_count * sizeof(uint32_t)));
        if (groups == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    uint32_t *right = groups + left_count;
    uint32_t *product = right + right_count;
    ft_big_number_read_groups(this->_digits, this->_size, groups);
    ft_big_number_read_groups(other._digits, other._size, right);
    ft_bzero(product, product_count * sizeof(uint32_t));
    ft_size_t left_index = 0U;
    while (left_index < left_count)
    {
        const uint32_t left_value = groups[left_index];
        if (left_value != 0U)
        {
            uint32_t *row = product + left_index;
            uint32_t carry = 0U;
            ft_size_t right_index = 0U;
            while (right_index < right_count)
            {
                const uint32_t value = left_value * right[right_index] + row[right_index] + carry;
                row[right_index] = value % FT_BIG_NUMBER_GROUP_BASE;
                carry = value / FT_BIG_NUMBER_GROUP_BASE;
                right_index += 1U;
            }
            row[right_count] = carry;
        }
        left_index += 1U;
    }
    result._size = ft_big_number_write_groups(result._digits, product, product_count);
    if (groups != inline_groups)
        cma_free(groups);
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::multiply_magnitude(const ft_big_number &other,
    ft_big_number &result) const noexcept
{
    constexpr ft_size_t GROUPED_MINIMUM_DIGITS = 32U;
    if (other._size >= SIZE_MAX || this->_size > SIZE_MAX - other._size - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    if (other._size == 1U)
        return (this->multiply_magnitude_by_digit(other._digits[0] - '0', result));
    if (this->_size == 1U)
        return (other.multiply_magnitude_by_digit(this->_digits[0] - '0', result));
    const ft_size_t result_length = this->_size + other._size;
    result.reserve(result_length + 1U);
    if (result._capacity <= result_length)
        return (FT_ERR_NO_MEMORY);
    if (this->_size >= GROUPED_MINIMUM_DIGITS && other._size >= GROUPED_MINIMUM_DIGITS)
        return (this->multiply_grouped_magnitude(other, result));

    ft_memset(result._digits, '0', result_length);
    result._digits[result_length] = '\0';
    result._size = result_length;
    ft_size_t left_index = this->_size;
    while (left_index != 0U)
    {
        left_index -= 1U;
        const uint32_t left_digit = static_cast<uint32_t>(this->_digits[left_index] - '0');
        if (left_digit != 0U)
        {
            char *row = result._digits + left_index + 1U;
            uint32_t carry = 0U;
            ft_size_t right_index = other._size;
            while (right_index != 0U)
            {
                right_index -= 1U;
                const uint32_t value = left_digit * static_cast<uint32_t>(other._digits[right_index] - '0')
                    + static_cast<uint32_t>(row[right_index] - '0') + carry;
                row[right_index] = static_cast<char>('0' + value % 10U);
                carry = value / 10U;
            }
            result._digits[left_index] = static_cast<char>('0' + carry);
        }
    }
    result.trim_leading_zeros_unlocked();
    return (FT_ERR_SUCCESS);
}

ft_big_number_proxy ft_big_number::operator*(const ft_big_number &other) const noexcept
{
    ft_big_number_proxy result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t error_code = result._value.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = ft_big_number::lock_pair(*this, other, lower, upper);
    if (error_code == FT_ERR_SUCCESS)
    {
        if (this->is_zero_value() == FT_FALSE && other.is_zero_value() == FT_FALSE)
        {
            error_code = this->multiply_magnitude(other, result._value);
            if (error_code == FT_ERR_SUCCESS)
                result._value._is_negative = this->_is_negative != other._is_negative;
            else
                result._value.clear_unlocked();
        }
        (void)ft_big_number::unlock_pair(lower, upper);
    }
    result._last_error = ft_big_number::set_error(error_code);
    return (result);
}

static ft_size_t ft_big_number_store_digit_product(char *product, const char *digits,
    ft_size_t size, uint32_t digit) noexcept
{
    ft_size_t index = size;
    uint32_t carry = 0U;
    while (index > 0U)
    {
        const uint32_t value = static_cast<uint32_t>(digits[index - 1U] - '0')
            * digit + carry;
        product[index] = static_cast<char>('0' + value % 10U);
        carry = value / 10U;
        index -= 1U;
    }
    if (carry != 0U)
    {
        product[0] = static_cast<char>('0' + carry);
        size += 1U;
    }
    else
        ft_memmove(product, product + 1, size);
    product[size] = '\0';
    return (size);
}

int32_t ft_big_number::multiply_magnitude_by_digit(uint32_t digit,
    ft_big_number &result) const noexcept
{
    if (digit == 1U)
        return (this->copy_magnitude(result));
    result.reserve(this->_size + 2U);
    if (result._capacity < this->_size + 2U)
        return (FT_ERR_NO_MEMORY);
    result._size = ft_big_number_store_digit_product(result._digits, this->_digits,
        this->_size, digit);
    return (FT_ERR_SUCCESS);
}

int32_t ft_big_number::divide_by_digit(uint32_t divisor, ft_big_number *quotient,
    ft_big_number &remainder) const noexcept
{
    uint32_t carry = 0U;
    ft_size_t index = 0U;
    while (index < this->_size)
    {
        const uint32_t value = carry * 10U
            + static_cast<uint32_t>(this->_digits[index] - '0');
        carry = value % divisor;
        if (quotient != ft_nullptr)
            quotient->_digits[index] = static_cast<char>('0' + value / divisor);
        index += 1U;
    }
    if (quotient == ft_nullptr && carry != 0U)
    {
        remainder.reserve(2U);
        if (remainder._capacity < 2U)
            return (FT_ERR_NO_MEMORY);
        remainder._digits[0] = static_cast<char>('0' + carry);
        remainder._digits[1] = '\0';
        remainder._size = 1U;
    }
    return (FT_ERR_SUCCESS);
}

struct ft_big_number_divisor_product
{
    const char *digits;
    ft_size_t size;
};

int32_t ft_big_number::divide_magnitude(const ft_big_number &divisor,
    ft_big_number *quotient, ft_big_number &remainder) const noexcept
{
    if (quotient != ft_nullptr)
    {
        quotient->reserve(this->_size + 1U);
        if (quotient->_capacity <= this->_size)
            return (FT_ERR_NO_MEMORY);
    }
    if (divisor._size == 1U)
        return (this->divide_by_digit(static_cast<uint32_t>(divisor._digits[0] - '0'),
            quotient, remainder));
    constexpr ft_size_t PRODUCT_COUNT = 8U;
    if (divisor._size > SIZE_MAX / PRODUCT_COUNT - 2U)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t stride = divisor._size + 2U;
    // A partial remainder is below the divisor before the next digit is appended.
    remainder.reserve(stride);
    if (remainder._capacity < stride)
        return (FT_ERR_NO_MEMORY);

    char inline_products[1024];
    char *storage = inline_products;
    if (stride * PRODUCT_COUNT > sizeof(inline_products))
    {
        storage = static_cast<char *>(cma_malloc(stride * PRODUCT_COUNT));
        if (storage == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    ft_big_number_divisor_product products[10];
    products[1] = {divisor._digits, divisor._size};
    uint32_t digit = 2U;
    while (digit <= 9U)
    {
        char *product = storage + (digit - 2U) * stride;
        const ft_size_t size = ft_big_number_store_digit_product(product,
            divisor._digits, divisor._size, digit);
        products[digit] = {product, size};
        digit += 1U;
    }
    ft_size_t index = 0U;
    while (index < this->_size)
    {
        remainder.store_appended_digit(remainder.append_position(), this->_digits[index]);
        uint32_t best_digit = 0U;
        if (remainder.compare_magnitude(divisor) >= 0)
        {
            best_digit = 1U;
            uint32_t upper_digit = 9U;
            while (best_digit < upper_digit)
            {
                const uint32_t candidate = (best_digit + upper_digit + 1U) / 2U;
                const ft_big_number_divisor_product &product = products[candidate];
                if (remainder._size > product.size
                    || (remainder._size == product.size
                        && ft_memcmp(remainder._digits, product.digits, product.size) >= 0))
                    best_digit = candidate;
                else
                    upper_digit = candidate - 1U;
            }
            const ft_big_number_divisor_product &product = products[best_digit];
            ft_big_number_subtract_digits(remainder._digits, remainder._digits,
                remainder._size, product.digits, product.size);
            remainder.trim_leading_zeros_unlocked(FT_FALSE);
        }
        if (quotient != ft_nullptr)
            quotient->_digits[index] = static_cast<char>('0' + best_digit);
        index += 1U;
    }
    if (storage != inline_products)
        cma_free(storage);
    return (FT_ERR_SUCCESS);
}

ft_big_number_proxy ft_big_number::quotient_or_remainder(
    const ft_big_number &divisor, ft_bool remainder_only) const noexcept
{
    ft_big_number_proxy result;
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    int32_t error_code = result._value.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = ft_big_number::lock_pair(*this, divisor, lower, upper);
    if (error_code == FT_ERR_SUCCESS)
    {
        if (divisor.is_zero_value() == FT_TRUE)
            error_code = FT_ERR_DIVIDE_BY_ZERO;
        else if (this->is_zero_value() == FT_FALSE)
        {
            const int32_t comparison = this->compare_magnitude(divisor);
            if (remainder_only == FT_TRUE)
            {
                if (comparison < 0)
                    error_code = this->copy_magnitude(result._value);
                else if (comparison > 0)
                    error_code = this->divide_magnitude(divisor, ft_nullptr, result._value);
            }
            else if (comparison == 0)
            {
                result._value.assign("1");
                error_code = ft_big_number::_last_error;
            }
            else if (comparison > 0)
            {
                ft_big_number remainder;
                error_code = remainder.initialize();
                if (error_code == FT_ERR_SUCCESS)
                    error_code = this->divide_magnitude(divisor, &result._value, remainder);
                if (error_code == FT_ERR_SUCCESS)
                {
                    result._value._size = this->_size;
                    result._value._digits[this->_size] = '\0';
                    result._value.trim_leading_zeros_unlocked();
                }
            }
            if (error_code == FT_ERR_SUCCESS)
            {
                if (result._value.is_zero_value() == FT_TRUE)
                    result._value.clear_unlocked();
                else if (remainder_only == FT_TRUE)
                    result._value._is_negative = this->_is_negative;
                else
                    result._value._is_negative = this->_is_negative != divisor._is_negative;
            }
        }
        if (error_code != FT_ERR_SUCCESS)
            result._value.clear_unlocked();
        (void)ft_big_number::unlock_pair(lower, upper);
    }
    result._last_error = ft_big_number::set_error(error_code);
    return (result);
}

ft_big_number_proxy ft_big_number::operator/(const ft_big_number &other) const noexcept
{
    return (this->quotient_or_remainder(other, FT_FALSE));
}

ft_big_number_proxy ft_big_number::operator%(const ft_big_number &other) const noexcept
{
    return (this->quotient_or_remainder(other, FT_TRUE));
}

int32_t ft_big_number::compare_value(const ft_big_number &other, int32_t &comparison) const noexcept
{
    const ft_big_number *lower = ft_nullptr;
    const ft_big_number *upper = ft_nullptr;
    const int32_t lock_error = ft_big_number::lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const ft_bool left_negative = this->_is_negative == FT_TRUE && this->is_zero_value() == FT_FALSE;
    const ft_bool right_negative = other._is_negative == FT_TRUE && other.is_zero_value() == FT_FALSE;
    if (left_negative != right_negative)
        comparison = static_cast<int32_t>(right_negative) - static_cast<int32_t>(left_negative);
    else
    {
        comparison = this->compare_magnitude(other);
        if (left_negative == FT_TRUE)
            comparison = -comparison;
    }
    (void)ft_big_number::unlock_pair(lower, upper);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_big_number::operator==(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison == 0);
}

ft_bool ft_big_number::operator!=(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison != 0);
}

ft_bool ft_big_number::operator<(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison < 0);
}

ft_bool ft_big_number::operator<=(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison <= 0);
}

ft_bool ft_big_number::operator>(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison > 0);
}

ft_bool ft_big_number::operator>=(const ft_big_number &other) const noexcept
{
    int32_t comparison;
    const int32_t error_code = this->compare_value(other, comparison);
    return (ft_big_number::set_error(error_code) == FT_ERR_SUCCESS && comparison >= 0);
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

static ft_size_t ft_big_number_format_expansion(uint32_t base) noexcept
{
    if (base >= 10U)
        return (1U);
    if (base >= 4U)
        return (2U);
    if (base == 3U)
        return (3U);
    return (4U);
}

static ft_size_t ft_big_number_format_groups(char *output, uint32_t *groups,
    ft_size_t count, uint32_t base) noexcept
{
    // A group remainder times the decimal group base fits in uint32_t.
    const uint32_t square = base * base;
    const uint32_t power = square * square;
    ft_size_t length = 0U;
    while (count != 0U)
    {
        uint32_t remainder = 0U;
        ft_size_t index = count;
        while (index != 0U)
        {
            index -= 1U;
            const uint32_t value = remainder * FT_BIG_NUMBER_GROUP_BASE + groups[index];
            groups[index] = value / power;
            remainder = value % power;
        }
        while (count != 0U && groups[count - 1U] == 0U)
            count -= 1U;
        uint32_t digits_written = 0U;
        while (digits_written < FT_BIG_NUMBER_GROUP_DIGITS
            && (count != 0U || remainder != 0U))
        {
            output[length] = FT_BIG_NUMBER_DIGITS[remainder % base];
            length += 1U;
            remainder /= base;
            digits_written += 1U;
        }
    }
    ft_size_t left = 0U;
    ft_size_t right = length;
    while (left < right)
    {
        right -= 1U;
        const char digit = output[left];
        output[left] = output[right];
        output[right] = digit;
        left += 1U;
    }
    output[length] = '\0';
    return (length);
}

static int32_t ft_big_number_format_nondecimal(const char *digits, ft_size_t digit_count,
    ft_bool negative, uint32_t base, char *&output, ft_size_t &length) noexcept
{
    constexpr ft_size_t INLINE_GROUPS = 128U;
    uint32_t inline_groups[INLINE_GROUPS];
    const ft_size_t expansion = ft_big_number_format_expansion(base);
    const ft_size_t sign = negative == FT_TRUE;
    if (digit_count > (SIZE_MAX - sign - 1U) / expansion)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t group_count = ft_big_number_group_count(digit_count);
    if (group_count > SIZE_MAX / sizeof(uint32_t))
        return (FT_ERR_OUT_OF_RANGE);
    uint32_t *groups = inline_groups;
    if (group_count > INLINE_GROUPS)
    {
        groups = static_cast<uint32_t *>(cma_malloc(group_count * sizeof(uint32_t)));
        if (groups == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    output = static_cast<char *>(cma_malloc(digit_count * expansion + sign + 1U));
    if (output != ft_nullptr)
    {
        if (sign != 0U)
            output[0] = '-';
        ft_big_number_read_groups(digits, digit_count, groups);
        length = sign + ft_big_number_format_groups(output + sign, groups, group_count, base);
    }
    if (groups != inline_groups)
        cma_free(groups);
    if (output == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

ft_string *ft_big_number::format_base(int32_t base) const noexcept
{
    if (base < 2 || base > 16)
    {
        ft_big_number::set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_string::from_error(FT_ERR_INVALID_ARGUMENT));
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(lock_error);
        return (ft_string::from_error(lock_error));
    }

    const char *digits = "0";
    ft_size_t digit_count = 1U;
    ft_bool negative = FT_FALSE;
    const ft_bool zero = this->is_zero_value();
    if (zero == FT_FALSE)
    {
        ft_size_t first_digit = 0U;
        while (this->_digits[first_digit] == '0')
            first_digit += 1U;
        digits = this->_digits + first_digit;
        digit_count = this->_size - first_digit;
        negative = this->_is_negative;
    }
    char *output = ft_nullptr;
    ft_size_t length = 0U;
    int32_t error_code = FT_ERR_SUCCESS;
    if (base == 10 || zero == FT_TRUE)
    {
        const ft_size_t sign = negative == FT_TRUE;
        if (digit_count > SIZE_MAX - sign - 1U)
            error_code = FT_ERR_OUT_OF_RANGE;
        else
        {
            length = digit_count + sign;
            output = static_cast<char *>(cma_malloc(length + 1U));
            if (output == ft_nullptr)
                error_code = FT_ERR_NO_MEMORY;
            else
            {
                if (sign != 0U)
                    output[0] = '-';
                ft_memcpy(output + sign, digits, digit_count);
                output[length] = '\0';
            }
        }
    }
    else
        error_code = ft_big_number_format_nondecimal(digits, digit_count, negative,
            static_cast<uint32_t>(base), output, length);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_string::from_error(error_code));
    }

    ft_string *result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
    {
        cma_free(output);
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    error_code = ft_string_take_owned_buffer(*result, output, length);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(output);
        delete result;
        ft_big_number::set_error(error_code);
        return (ft_string::from_error(error_code));
    }
    ft_big_number::set_error(FT_ERR_SUCCESS);
    return (result);
}

ft_string *ft_big_number::to_string_base(int32_t base) noexcept
{
    return (this->format_base(base));
}

ft_bool ft_big_number::halve_exponent() noexcept
{
    uint32_t carry = 0U;
    ft_size_t index = 0U;
    while (index < this->_size)
    {
        const uint32_t value = carry * 10U + static_cast<uint32_t>(this->_digits[index] - '0');
        this->_digits[index] = static_cast<char>('0' + value / 2U);
        carry = value % 2U;
        index += 1U;
    }
    this->trim_leading_zeros_unlocked(FT_FALSE);
    if (this->_size == 1U && this->_digits[0] == '0')
    {
        this->_digits[0] = '\0';
        this->_size = 0U;
    }
    return (carry != 0U);
}

int32_t ft_big_number::mod_pow_value(const ft_big_number &exponent,
    const ft_big_number &modulus, ft_big_number &result) const noexcept
{
    ft_big_number exponent_value;
    ft_big_number modulus_value;
    int32_t error_code = exponent_value.initialize(exponent);
    if (error_code == FT_ERR_SUCCESS)
        error_code = modulus_value.initialize(modulus);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (modulus_value.is_zero_value() == FT_TRUE)
        return (FT_ERR_DIVIDE_BY_ZERO);
    if (modulus_value._is_negative == FT_TRUE
        || (exponent_value._is_negative == FT_TRUE && exponent_value.is_zero_value() == FT_FALSE))
        return (FT_ERR_INVALID_ARGUMENT);
    if (modulus_value._size == 1U && modulus_value._digits[0] == '1')
        return (FT_ERR_SUCCESS);
    result.assign("1");
    if (ft_big_number::_last_error != FT_ERR_SUCCESS || exponent_value.is_zero_value() == FT_TRUE)
        return (ft_big_number::_last_error);

    ft_big_number base;
    ft_big_number product;
    error_code = base.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = product.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    const auto store_value = [](ft_big_number &destination, const ft_big_number_proxy &value) noexcept
    {
        const int32_t proxy_error = value.get_error();
        if (proxy_error != FT_ERR_SUCCESS)
            return (proxy_error);
        destination = value;
        return (ft_big_number::_last_error);
    };
    error_code = store_value(base, *this % modulus_value);
    if (error_code == FT_ERR_SUCCESS && base._is_negative == FT_TRUE)
        error_code = store_value(base, base + modulus_value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    if (base.is_zero_value() == FT_TRUE)
    {
        result.clear_unlocked();
        return (FT_ERR_SUCCESS);
    }
    while (exponent_value._size != 0U)
    {
        const ft_bool odd = exponent_value.halve_exponent();
        if (odd == FT_TRUE)
        {
            error_code = store_value(product, result * base);
            if (error_code == FT_ERR_SUCCESS)
                error_code = store_value(result, product % modulus_value);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        if (exponent_value._size == 0U)
            break ;
        error_code = store_value(product, base * base);
        if (error_code == FT_ERR_SUCCESS)
            error_code = store_value(base, product % modulus_value);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

ft_big_number *ft_big_number::mod_pow(const ft_big_number &exponent,
    const ft_big_number &modulus) const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "ft_big_number::mod_pow", "called with uninitialised base");
        ft_big_number::set_error(FT_ERR_INVALID_STATE);
        return (ft_nullptr);
    }
    ft_big_number *result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    int32_t error_code = result->initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(error_code);
        return (ft_nullptr);
    }
    error_code = this->mod_pow_value(exponent, modulus, *result);
    if (error_code != FT_ERR_SUCCESS)
    {
        result->clear_unlocked();
        result->_operation_error = error_code;
    }
    ft_big_number::set_error(error_code);
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
    if (this->_value._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t operation_error = ft_big_number::_last_error;
        if (operation_error != FT_ERR_SUCCESS)
        {
            (void)this->_value.destroy();
            ft_big_number::set_error(operation_error);
        }
    }
    return ;
}

ft_big_number_proxy ft_big_number_proxy::operator+(const ft_big_number &right) const noexcept
{
    const int32_t error_code = this->get_error();
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_big_number_proxy(error_code));
    }
    return (this->_value + right);
}

ft_big_number_proxy ft_big_number_proxy::operator-(const ft_big_number &right) const noexcept
{
    const int32_t error_code = this->get_error();
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_big_number_proxy(error_code));
    }
    return (this->_value - right);
}

ft_big_number_proxy ft_big_number_proxy::operator*(const ft_big_number &right) const noexcept
{
    const int32_t error_code = this->get_error();
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_big_number_proxy(error_code));
    }
    return (this->_value * right);
}

ft_big_number_proxy ft_big_number_proxy::operator/(const ft_big_number &right) const noexcept
{
    const int32_t error_code = this->get_error();
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_big_number_proxy(error_code));
    }
    return (this->_value / right);
}

ft_big_number_proxy ft_big_number_proxy::operator%(const ft_big_number &right) const noexcept
{
    const int32_t error_code = this->get_error();
    if (error_code != FT_ERR_SUCCESS)
    {
        ft_big_number::set_error(error_code);
        return (ft_big_number_proxy(error_code));
    }
    return (this->_value % right);
}

ft_big_number_proxy::operator ft_big_number *() const noexcept
{
    ft_big_number *result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    const int32_t proxy_error = this->get_error();
    int32_t initialization_error;
    if (proxy_error == FT_ERR_SUCCESS)
        initialization_error = result->initialize(this->_value);
    else
        initialization_error = result->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(initialization_error);
        return (ft_nullptr);
    }
    ft_big_number::set_error(proxy_error);
    return (result);
}

int32_t ft_big_number_proxy::get_error() const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
        return (this->_last_error);
    if (this->_value._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
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
