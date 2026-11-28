#include "class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include <climits>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/move.hpp"

void ft_string::sleep_backoff() noexcept
{
    pt_thread_sleep(1);
    return ;
}

int32_t ft_string::enable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_string::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (this->set_error(FT_ERR_NO_MEMORY));
    int32_t mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (this->set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_string::disable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_string::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (this->set_error(destroy_error));
}

ft_bool ft_string::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_string::is_thread_safe");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_string::set_error(int32_t error_code) const noexcept
{
    this->_last_error = error_code;
    return (error_code);
}

int32_t ft_string::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state,
            "ft_string::get_error");
    return (this->_last_error);
}

const char *ft_string::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state,
            "ft_string::get_error_str");
    return (ft_strerror(this->get_error()));
}


int32_t ft_string::resize_buffer(ft_size_t new_capacity) noexcept
{
    if (new_capacity <= this->_capacity)
        return (FT_ERR_SUCCESS);
    if (new_capacity == FT_SYSTEM_SIZE_MAX)
        return (FT_ERR_NO_MEMORY);
    char* new_data = static_cast<char*>(cma_realloc(this->_data, new_capacity + 1));
    if (!new_data)
        return (FT_ERR_NO_MEMORY);
    this->_data = new_data;
    this->_capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::reserve_for_length(ft_size_t length, ft_size_t initial_capacity) noexcept
{
    if (length < this->_capacity)
        return (FT_ERR_SUCCESS);
    if (length == FT_SYSTEM_SIZE_MAX)
        return (FT_ERR_NO_MEMORY);
    ft_size_t new_capacity = this->_capacity;
    if (new_capacity == 0)
        new_capacity = initial_capacity;
    while (new_capacity <= length)
    {
        if (new_capacity > (FT_SYSTEM_SIZE_MAX - 1) / 2)
        {
            new_capacity = length;
            break ;
        }
        new_capacity *= 2;
    }
    return (this->resize_buffer(new_capacity));
}

int32_t ft_string::append_char_buffer(char character) noexcept
{
    if (this->_length + 1 >= this->_capacity)
    {
        const int32_t resize_error = this->reserve_for_length(this->_length + 1, 1);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
    }
    this->_data[this->_length++] = character;
    this->_data[this->_length] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::append_buffer(const char *string, ft_size_t length) noexcept
{
    if (!string || length == 0)
        return (FT_ERR_SUCCESS);
    if (length >= FT_SYSTEM_SIZE_MAX - this->_length)
        return (FT_ERR_NO_MEMORY);
    const ft_size_t new_length = this->_length + length;
    if (new_length >= this->_capacity)
    {
        const uintptr_t source_address = reinterpret_cast<uintptr_t>(string);
        const uintptr_t data_address = reinterpret_cast<uintptr_t>(this->_data);
        const ft_bool aliases_storage = this->_data != ft_nullptr
            && source_address >= data_address && source_address - data_address <= this->_capacity;
        const int32_t resize_error = this->reserve_for_length(new_length, 1);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
        if (aliases_storage == FT_TRUE)
            string = this->_data + (source_address - data_address);
    }
    (void)ft_memmove(this->_data + this->_length, string, length);
    this->_length = new_length;
    this->_data[this->_length] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::clear_buffer() noexcept
{
    this->_length = 0;
    if (this->_data)
        this->_data[0] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::ensure_empty_buffer() noexcept
{
    char *new_data;

    if (this->_data)
    {
        this->_data[0] = '\0';
        if (this->_capacity == 0)
            this->_capacity = 1;
        this->_length = 0;
        return (FT_ERR_SUCCESS);
    }
    new_data = static_cast<char*>(cma_malloc(1));
    if (!new_data)
        return (FT_ERR_NO_MEMORY);
    new_data[0] = '\0';
    this->_data = new_data;
    this->_capacity = 1;
    this->_length = 0;
    this->_data[0] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::assign_buffer(ft_size_t count, char character) noexcept
{
    if (count == 0)
        return (this->ensure_empty_buffer());
    const int32_t resize_error = this->reserve_for_length(count, 16);
    if (resize_error != FT_ERR_SUCCESS)
        return (resize_error);
    ft_size_t index = 0;
    while (index < count)
    {
        this->_data[index] = character;
        ++index;
    }
    this->_length = count;
    this->_data[this->_length] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::assign_buffer(const char *string, ft_size_t length) noexcept
{
    if (length == 0)
    {
        int32_t clear_error = this->clear_buffer();
        if (clear_error != FT_ERR_SUCCESS)
            return (clear_error);
        return (this->ensure_empty_buffer());
    }
    if (!string)
        return (FT_ERR_INVALID_ARGUMENT);
    if (length == FT_SYSTEM_SIZE_MAX)
        return (FT_ERR_NO_MEMORY);
    if (this->_capacity <= length)
    {
        ft_size_t new_capacity = length;
        if (length <= (FT_SYSTEM_SIZE_MAX - 1) / 2)
            new_capacity *= 2;
        const uintptr_t source_address = reinterpret_cast<uintptr_t>(string);
        const uintptr_t data_address = reinterpret_cast<uintptr_t>(this->_data);
        const ft_bool aliases_storage = this->_data != ft_nullptr
            && source_address >= data_address && source_address - data_address <= this->_capacity;
        const int32_t resize_error = this->resize_buffer(new_capacity);
        if (resize_error != FT_ERR_SUCCESS)
            return (resize_error);
        if (aliases_storage == FT_TRUE)
            string = this->_data + (source_address - data_address);
    }
    (void)ft_memmove(this->_data, string, length);
    this->_length = length;
    this->_data[length] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::erase_buffer(ft_size_t index, ft_size_t count) noexcept
{
    if (index >= this->_length)
        return (FT_ERR_OUT_OF_RANGE);
    if (count > this->_length - index)
        count = this->_length - index;
    if (count > 0)
    {
        ft_memmove(this->_data + index, this->_data + index + count,
                this->_length - index - count);
        this->_length -= count;
        this->_data[this->_length] = '\0';
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::resize_length_buffer(ft_size_t new_length) noexcept
{
    const int32_t reserve_error = this->reserve_for_length(new_length, 16);
    if (reserve_error != FT_ERR_SUCCESS)
        return (reserve_error);
    ft_size_t index = this->_length;
    while (index < new_length)
    {
        this->_data[index] = '\0';
        ++index;
    }
    this->_length = new_length;
    this->_data[this->_length] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::move_buffer(ft_string &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    cma_free(this->_data);
    this->_data = other._data;
    this->_length = other._length;
    this->_capacity = other._capacity;
    if (!this->_data)
        this->_capacity = 0;
    other._data = ft_nullptr;
    other._length = 0;
    other._capacity = 0;
    return (FT_ERR_SUCCESS);
}

int32_t ft_string::resize(ft_size_t new_capacity) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::resize");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->resize_buffer(new_capacity);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(result));
    }
    int32_t result = this->resize_buffer(new_capacity);
    return (this->set_error(result));
}

int32_t ft_string::append(char character) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::append(char)");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->append_char_buffer(character);
        this->set_error(result);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (result);
    }
    int32_t result = this->append_char_buffer(character);
    return (this->set_error(result));
}

int32_t ft_string::append(const ft_string& string) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::append(const ft_string &)");
    errno_abort_if_uninitialised_or_destroyed(string._initialised_state, "ft_string::append(const ft_string &)");
    pt_recursive_mutex *first = this->_mutex;
    pt_recursive_mutex *second = string._mutex;
    if (first == second)
        second = ft_nullptr;
    else if (reinterpret_cast<uintptr_t>(first) > reinterpret_cast<uintptr_t>(second))
    {
        first = string._mutex;
        second = this->_mutex;
    }
    int32_t result = pt_recursive_mutex_lock_if_not_null(first);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = pt_recursive_mutex_lock_if_not_null(second);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->append_buffer(string._data, string._length);
        (void)pt_recursive_mutex_unlock_if_not_null(second);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(first);
    return (this->set_error(result));
}

int32_t ft_string::append(const char *string) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::append(const char *)");
    if (string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->append(string, ft_strlen_size_t(string)));
}

int32_t ft_string::clear() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::clear");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        int32_t result = this->clear_buffer();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (result);
    }
    return (this->clear_buffer());
}

const char* ft_string::at(ft_size_t index) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::at");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (ft_nullptr);
        if (index >= this->_length)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (ft_nullptr);
        }
        const char *value = &this->_data[index];
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (value);
    }
    if (index >= this->_length)
        return (ft_nullptr);
    return (&this->_data[index]);
}

const char* ft_string::c_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::c_str");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (const_cast<char *>(""));
        const char *value;

        if (this->_data != ft_nullptr)
            value = this->_data;
        else
            value = const_cast<char *>("");
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (value);
    }
    if (this->_data)
        return (this->_data);
    return (const_cast<char *>(""));
}

char* ft_string::data() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::data");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (ft_nullptr);
        char *value = this->_data;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (value);
    }
    return (this->_data);
}

const char* ft_string::data() const noexcept
{
    return (this->c_str());
}

char* ft_string::print() noexcept
{
    return (this->data());
}

ft_size_t ft_string::size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::size");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (0);
        ft_size_t length = this->_length;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (length);
    }
    return (this->_length);
}

ft_bool ft_string::empty() const noexcept
{
    return (this->size() == 0);
}

ft_bool ft_string::is_initialised() const noexcept
{
    return (this->_initialised_state == FT_CLASS_STATE_INITIALISED);
}

int32_t ft_string::move(ft_string& other) noexcept
{
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
            errno_abort_lifecycle(other._initialised_state, "ft_string::move source",
                "called with uninitialised source object");
        else
            errno_abort_lifecycle(other._initialised_state, "ft_string::move source",
                "called with destroyed source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        int32_t initialize_error = this->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    ft_bool self_locked = FT_FALSE;
    ft_bool other_locked = FT_FALSE;
    pt_recursive_mutex *first = this->_mutex;
    pt_recursive_mutex *second = other._mutex;
    if (first != ft_nullptr && second != ft_nullptr)
    {
        ft_bool first_is_self = FT_TRUE;
        ft_bool second_is_self = FT_FALSE;
        if (first > second)
        {
            pt_recursive_mutex *temporary = first;
            first = second;
            second = temporary;
            first_is_self = FT_FALSE;
            second_is_self = FT_TRUE;
        }
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(first);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        if (first_is_self)
            self_locked = FT_TRUE;
        else
            other_locked = FT_TRUE;
        lock_error = pt_recursive_mutex_lock_if_not_null(second);
        if (lock_error != FT_ERR_SUCCESS)
        {
            if (second_is_self)
                self_locked = FT_TRUE;
            else
                other_locked = FT_TRUE;
            if (self_locked)
                (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            if (other_locked && other._mutex != this->_mutex)
                (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
            return (lock_error);
        }
        if (second_is_self)
            self_locked = FT_TRUE;
        else
            other_locked = FT_TRUE;
    }
    else if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        self_locked = FT_TRUE;
    }
    else if (other._mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        other_locked = FT_TRUE;
    }
    int32_t result = this->move_buffer(other);
    if (other_locked && other._mutex != this->_mutex)
        (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    if (self_locked)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

int32_t ft_string::erase(ft_size_t index, ft_size_t count) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::erase");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->erase_buffer(index, count);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(result));
    }
    int32_t result = this->erase_buffer(index, count);
    return (this->set_error(result));
}

int32_t ft_string::append(const char *string, ft_size_t length) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::append(const char *, ft_size_t)");
    if (!string)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (length == 0)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->append_buffer(string, length);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(result));
    }
    int32_t result = this->append_buffer(string, length);
    return (this->set_error(result));
}

int32_t ft_string::assign(ft_size_t count, char character) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::assign(ft_size_t, char)");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->assign_buffer(count, character);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(result));
    }
    int32_t result = this->assign_buffer(count, character);
    return (this->set_error(result));
}

int32_t ft_string::assign(const char *string, ft_size_t length) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::assign(const char *, ft_size_t)");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (this->set_error(lock_error));
        int32_t result = this->assign_buffer(string, length);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(result));
    }
    int32_t result = this->assign_buffer(string, length);
    return (this->set_error(result));
}

int32_t ft_string::assign(const ft_string &other) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_string::assign(const ft_string &) destination");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "ft_string::assign(const ft_string &) source");
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));

    pt_recursive_mutex *first = this->_mutex;
    pt_recursive_mutex *second = other._mutex;
    if (first == second)
        second = ft_nullptr;
    else if (reinterpret_cast<uintptr_t>(first) > reinterpret_cast<uintptr_t>(second))
    {
        first = other._mutex;
        second = this->_mutex;
    }
    int32_t result = pt_recursive_mutex_lock_if_not_null(first);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = pt_recursive_mutex_lock_if_not_null(second);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->resize_buffer(other._length);
        if (result == FT_ERR_SUCCESS)
        {
            if (other._length != 0U)
                ft_memcpy(this->_data, other._data, other._length);
            this->_length = other._length;
            if (this->_data != ft_nullptr)
                this->_data[this->_length] = '\0';
        }
        (void)pt_recursive_mutex_unlock_if_not_null(second);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(first);
    return (this->set_error(result));
}

int32_t ft_string::resize_length(ft_size_t new_length) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::resize_length");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        int32_t result = this->resize_length_buffer(new_length);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (result);
    }
    return (this->resize_length_buffer(new_length));
}

int32_t ft_string_take_owned_buffer(ft_string &output, char *buffer, ft_size_t length,
    char **previous_buffer) noexcept
{
    if (buffer == ft_nullptr || length == FT_SYSTEM_SIZE_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    if (output.is_initialised() == FT_FALSE)
    {
        const int32_t initialize_error = output.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(output._mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    char *previous = output._data;
    output._data = buffer;
    output._length = length;
    output._capacity = length;
    output.set_error(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(output._mutex);
    if (previous == buffer)
        previous = ft_nullptr;
    if (previous_buffer != ft_nullptr)
        *previous_buffer = previous;
    else if (previous != ft_nullptr)
        cma_free(previous);
    return (FT_ERR_SUCCESS);
}

int32_t ft_string_take_owned_buffer(ft_string &output, char *buffer, ft_size_t length) noexcept
{
    return (ft_string_take_owned_buffer(output, buffer, length, ft_nullptr));
}

int32_t ft_string::push_back(char character) noexcept
{
    return (this->append(character));
}

char ft_string::back() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::back");
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return ('\0');
        if (this->_length == 0)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return ('\0');
        }
        char result = this->_data[this->_length - 1];
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (result);
    }
    if (this->_length == 0)
        return ('\0');
    return (this->_data[this->_length - 1]);
}

static ft_size_t string_find_bytes(const char *data, ft_size_t length,
    const char *substring, ft_size_t substring_length) noexcept
{
    if (substring_length == 0)
        return (0);
    if (data == substring && substring_length <= length)
        return (0);
    const char *match = static_cast<const char *>(
        ft_memmem(data, length, substring, substring_length));
    if (match == ft_nullptr)
        return (ft_string::npos);
    return (static_cast<ft_size_t>(match - data));
}

ft_size_t ft_string::find(const char *substring) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::find(const char *)");
    if (substring == ft_nullptr)
        return (ft_string::npos);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (ft_string::npos);
    const ft_size_t result = string_find_bytes(this->_data, this->_length,
        substring, ft_strlen_size_t(substring));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

ft_size_t ft_string::find(const ft_string &substring) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::find(const ft_string &)");
    errno_abort_if_uninitialised_or_destroyed(substring._initialised_state, "ft_string::find(const ft_string &)");
    pt_recursive_mutex *first = this->_mutex;
    pt_recursive_mutex *second = substring._mutex;
    if (first == second)
        second = ft_nullptr;
    else if (reinterpret_cast<uintptr_t>(first) > reinterpret_cast<uintptr_t>(second))
    {
        first = substring._mutex;
        second = this->_mutex;
    }
    if (pt_recursive_mutex_lock_if_not_null(first) != FT_ERR_SUCCESS)
        return (ft_string::npos);
    ft_size_t result = ft_string::npos;
    if (pt_recursive_mutex_lock_if_not_null(second) == FT_ERR_SUCCESS)
    {
        result = string_find_bytes(this->_data, this->_length,
            substring._data, substring._length);
        (void)pt_recursive_mutex_unlock_if_not_null(second);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(first);
    return (result);
}

ft_string *ft_string::substr(ft_size_t index, ft_size_t count) const noexcept
{
    ft_string *substring;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::substr");
    substring = new (std::nothrow) ft_string();
    if (substring == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (substring->initialize() != FT_ERR_SUCCESS)
    {
        delete substring;
        this->set_error(FT_ERR_INITIALIZATION_FAILED);
        return (ft_nullptr);
    }
    if (this->_mutex != ft_nullptr)
    {
        int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
        {
            this->set_error(lock_error);
            (void)substring->destroy();
            delete substring;
            return (ft_nullptr);
        }
    }
    if (index > this->_length)
    {
        if (this->_mutex != ft_nullptr)
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (substring);
    }
    ft_size_t available_length = this->_length - index;
    ft_size_t copy_length = count;
    if (copy_length == ft_string::npos || copy_length > available_length)
        copy_length = available_length;
    if (copy_length > 0)
    {
        int32_t assignment_error = substring->assign(this->_data + index, copy_length);
        if (assignment_error != FT_ERR_SUCCESS)
        {
            if (this->_mutex != ft_nullptr)
                (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            (void)substring->destroy();
            delete substring;
            this->set_error(assignment_error);
            return (ft_nullptr);
        }
    }
    if (this->_mutex != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (substring);
}


ft_string::operator const char*() const noexcept
{
    return (this->c_str());
}

ft_string &ft_string::operator+=(const ft_string &string) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator+=(const ft_string &)");
    errno_abort_if_uninitialised_or_destroyed(string._initialised_state, "ft_string::operator+=(const ft_string &) source");
    this->set_error(this->append(string));
    return (*this);
}

ft_string &ft_string::operator+=(const char *string) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator+=(const char *)");
    this->set_error(this->append(string));
    return (*this);
}

ft_string &ft_string::operator+=(char character) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator+=(char)");
    this->set_error(this->append(character));
    return (*this);
}

ft_bool ft_string::operator==(const ft_string &other) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator==(const ft_string &)");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_string::operator==(const ft_string &) source");
    pt_recursive_mutex *first = this->_mutex;
    pt_recursive_mutex *second = other._mutex;
    if (first == second)
        second = ft_nullptr;
    else if (reinterpret_cast<uintptr_t>(first) > reinterpret_cast<uintptr_t>(second))
    {
        first = other._mutex;
        second = this->_mutex;
    }
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(first);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_FALSE);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(second);
    ft_bool equal = FT_FALSE;
    if (lock_error == FT_ERR_SUCCESS)
    {
        equal = this->_length == other._length
            && (this->_data == other._data
                || ft_memcmp(this->_data, other._data, this->_length) == 0);
        (void)pt_recursive_mutex_unlock_if_not_null(second);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(first);
    this->set_error(lock_error);
    return (equal);
}

ft_bool ft_string::operator==(const char *string) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator==(const char *)");
    if (string == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_FALSE);
    }
    ft_size_t index = 0;
    while (index < this->_length && string[index] != '\0'
        && this->_data[index] == string[index])
        ++index;
    const ft_bool equal = index == this->_length && string[index] == '\0';
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (equal);
}

ft_bool ft_string::operator!=(const ft_string &other) const noexcept
{
    return (!((*this) == other));
}

ft_bool ft_string::operator!=(const char *string) const noexcept
{
    return (!((*this) == string));
}

char ft_string::operator[](ft_size_t index) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string::operator[]");
    if (index >= this->_length)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return ('\0');
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_data[index]);
}

ft_string_proxy::ft_string_proxy() noexcept
    : _error_restorer()
    , _value()
    , _last_error(FT_ERR_SUCCESS)
{
    int32_t initialization_error;

    this->_error_restorer.set_error_pointer(&this->_last_error);
    initialization_error = this->_value.initialize("");
    if (initialization_error != FT_ERR_SUCCESS)
        this->_last_error = initialization_error;
    if (this->_value.get_error() != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    return ;
}

ft_string_proxy::ft_string_proxy(int32_t error_code) noexcept
    : _error_restorer()
    , _value()
    , _last_error(error_code)
{
    int32_t initialization_error;

    this->_error_restorer.set_error_pointer(&this->_last_error);
    initialization_error = this->_value.initialize("");
    if (initialization_error != FT_ERR_SUCCESS)
        this->_last_error = initialization_error;
    if (this->_value.get_error() != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    return ;
}

ft_string_proxy::ft_string_proxy(const ft_string &value, int32_t error_code) noexcept
    : _error_restorer()
    , _value()
    , _last_error(error_code)
{
    this->_error_restorer.set_error_pointer(&this->_last_error);
    if (this->_value.initialize(value) != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    if (this->_value.get_error() != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    return ;
}

ft_string_proxy::ft_string_proxy(ft_string &&value, int32_t error_code) noexcept
    : _error_restorer()
    , _value()
    , _last_error(error_code)
{
    this->_error_restorer.set_error_pointer(&this->_last_error);
    const int32_t value_error = this->_value.initialize(ft_move(value));
    if (value_error != FT_ERR_SUCCESS)
        this->_last_error = value_error;
    return ;
}

ft_string_proxy::ft_string_proxy(const ft_string_proxy &other) noexcept
    : _error_restorer()
    , _value()
    , _last_error(other._last_error)
{
    this->_error_restorer.set_error_pointer(&this->_last_error);
    if (this->_value.initialize(other._value) != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    if (this->_value.get_error() != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    return ;
}

ft_string_proxy::ft_string_proxy(ft_string_proxy &&other) noexcept
    : _error_restorer()
    , _value()
    , _last_error(other._last_error)
{
    this->_error_restorer.set_error_pointer(&this->_last_error);
    if (this->_value.initialize(static_cast<ft_string &&>(other._value))
        != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    other._last_error = FT_ERR_SUCCESS;
    if (this->_value.get_error() != FT_ERR_SUCCESS)
        this->_last_error = this->_value.get_error();
    return ;
}

ft_string_proxy::error_restorer::error_restorer() noexcept
    : _error_pointer(ft_nullptr)
{
    return ;
}

ft_string_proxy::error_restorer::error_restorer(
    const error_restorer &other) noexcept
    : _error_pointer(other._error_pointer)
{
    return ;
}

ft_string_proxy::error_restorer::~error_restorer() noexcept
{
    return ;
}

void ft_string_proxy::error_restorer::set_error_pointer(
    const int32_t *error_pointer) noexcept
{
    this->_error_pointer = error_pointer;
    return ;
}

ft_string_proxy::~ft_string_proxy()
{
    return ;
}

ft_string_proxy ft_string_proxy::operator+(const ft_string &right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
        return (ft_string_proxy(this->_last_error));
    return (this->_value + right);
}

ft_string_proxy ft_string_proxy::operator+(const char *right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
        return (ft_string_proxy(this->_last_error));
    return (this->_value + right);
}

ft_string_proxy ft_string_proxy::operator+(char right) const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
        return (ft_string_proxy(this->_last_error));
    return (this->_value + right);
}

ft_string_proxy::operator ft_string *() const noexcept
{
    if (this->_last_error != FT_ERR_SUCCESS)
        return (ft_string::from_error(this->_last_error));
    ft_string *result;

    result = new (std::nothrow) ft_string();
    if (result == ft_nullptr)
    {
        return (ft_nullptr);
    }
    if (result->initialize(this->_value) != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

int32_t ft_string_proxy::get_error() const noexcept
{
    return (this->_last_error);
}

ft_string &ft_string::operator=(const ft_string_proxy &other) noexcept
{
    if (other.get_error() != FT_ERR_SUCCESS)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            this->set_error(destroy_error);
        else
            this->set_error(other.get_error());
        return (*this);
    }
    ft_string value;
    if (value.initialize(other._value) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (*this);
    }
    return (this->operator=(ft_move(value)));
}

ft_string_proxy operator+(const ft_string &left, const ft_string &right) noexcept
{
    ft_string result;
    int32_t error_code;

    error_code = result.initialize(left);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    error_code = result.append(right);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    return (ft_string_proxy(ft_move(result), FT_ERR_SUCCESS));
}

ft_string_proxy operator+(const ft_string &left, const char *right) noexcept
{
    ft_string result;
    int32_t error_code;

    error_code = result.initialize(left);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    error_code = result.append(right);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    return (ft_string_proxy(ft_move(result), FT_ERR_SUCCESS));
}

ft_string_proxy operator+(const char *left, const ft_string &right) noexcept
{
    ft_string result;
    int32_t error_code;

    error_code = result.initialize(left);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    error_code = result.append(right);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    return (ft_string_proxy(ft_move(result), FT_ERR_SUCCESS));
}

ft_string_proxy operator+(const ft_string &left, char right) noexcept
{
    ft_string result;
    int32_t error_code;

    error_code = result.initialize(left);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    error_code = result.append(right);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    return (ft_string_proxy(ft_move(result), FT_ERR_SUCCESS));
}

ft_string_proxy operator+(char left, const ft_string &right) noexcept
{
    ft_string result;
    int32_t error_code;

    error_code = result.initialize(1, left);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    error_code = result.append(right);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_string_proxy(error_code));
    return (ft_string_proxy(ft_move(result), FT_ERR_SUCCESS));
}

ft_string_proxy operator+(const ft_string_proxy &left, const ft_string &right) noexcept
{
    return (left.operator+(right));
}

ft_string_proxy operator+(const ft_string_proxy &left, const char *right) noexcept
{
    return (left.operator+(right));
}

ft_string_proxy operator+(const ft_string_proxy &left, char right) noexcept
{
    return (left.operator+(right));
}

ft_bool operator==(const char *left, const ft_string &right) noexcept
{
    return (right == left);
}

ft_bool operator!=(const char *left, const ft_string &right) noexcept
{
    return (!(left == right));
}
