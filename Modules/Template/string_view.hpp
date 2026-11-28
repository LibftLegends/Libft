#ifndef FT_STRING_VIEW_HPP
#define FT_STRING_VIEW_HPP

#include <cstdint>
#include <type_traits>
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"

template <typename CharType>
class ft_string_view
{
    private:
        const CharType              *_data;
        ft_size_t                   _size;
        mutable pt_recursive_mutex  *_mutex;
        uint8_t                     _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;

    public:
        static const ft_size_t npos;

        class char_proxy
        {
            private:
                CharType _value;
                int32_t  _error;

            public:
                char_proxy(CharType value, int32_t error);
                operator CharType() const;
                int32_t get_error() const;
        };

        ft_string_view();
        ft_string_view(const ft_string_view &other) = delete;
        ft_string_view(ft_string_view &&other) = delete;
        ft_string_view(const CharType* string);
        ft_string_view(const CharType* string, ft_size_t size);
        ~ft_string_view();
        ft_string_view& operator=(const ft_string_view& other) = delete;
        ft_string_view& operator=(ft_string_view&& other) = delete;

        int32_t initialize();
        int32_t initialize(const CharType *string);
        int32_t initialize(const CharType *string, ft_size_t size);
        int32_t destroy();
        int32_t move(ft_string_view<CharType> &other);

        const CharType* data() const;
        ft_size_t size() const;
        ft_bool empty() const;
        char_proxy operator[](ft_size_t index) const;
        int32_t compare(const ft_string_view& other) const;
        int32_t substr(ft_size_t position, ft_size_t count, ft_string_view& out_view) const;
        int32_t substr(ft_size_t position, ft_string_view& out_view) const;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename CharType>
int32_t ft_string_view<CharType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename CharType>
int32_t ft_string_view<CharType>::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    lock_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_result != FT_ERR_SUCCESS)
        return (set_error(lock_result));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template <typename CharType>
int32_t ft_string_view<CharType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename CharType>
ft_string_view<CharType>::char_proxy::char_proxy(CharType value, int32_t error)
    : _value(value), _error(error)
{
    return ;
}

template <typename CharType>
ft_string_view<CharType>::char_proxy::operator CharType() const
{
    return (this->_value);
}

template <typename CharType>
int32_t ft_string_view<CharType>::char_proxy::get_error() const
{
    return (this->_error);
}

template <typename CharType>
ft_string_view<CharType>::ft_string_view()
    : _data(ft_nullptr), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename CharType>
ft_string_view<CharType>::ft_string_view(const CharType* string)
    : _data(ft_nullptr), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(string);
    (void)set_error(previous_error);
    return ;
}

template <typename CharType>
ft_string_view<CharType>::ft_string_view(const CharType* string, ft_size_t size)
    : _data(ft_nullptr), _size(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    uint32_t previous_error;

    previous_error = _last_error;
    (void)this->initialize(string, size);
    (void)set_error(previous_error);
    return ;
}

template <typename CharType>
ft_string_view<CharType>::~ft_string_view()
{
    uint32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename CharType>
int32_t ft_string_view<CharType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_string_view::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = ft_nullptr;
    this->_size = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
int32_t ft_string_view<CharType>::initialize(const CharType *string)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_string_view::initialize(string)", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = string;
    this->_size = 0;
    if (string != ft_nullptr)
    {
        if constexpr (std::is_same_v<CharType, char>)
            this->_size = ft_strlen_size_t(string);
        else
        {
            ft_size_t length = 0U;
            while (string[length] != CharType())
                ++length;
            this->_size = length;
        }
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
int32_t ft_string_view<CharType>::initialize(const CharType *string, ft_size_t size)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "ft_string_view::initialize(string,size)",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = string;
    this->_size = size;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
int32_t ft_string_view<CharType>::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t first_error;
    int32_t disable_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_result = this->disable_thread_safety();
    if (disable_result != FT_ERR_SUCCESS)
        first_error = disable_result;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = lock_error;
    this->_data = ft_nullptr;
    this->_size = 0;
    (void)this->unlock_internal(lock_acquired);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename CharType>
int32_t ft_string_view<CharType>::move(ft_string_view<CharType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_string_view::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_data = ft_nullptr;
        this->_size = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_data = other._data;
    this->_size = other._size;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._size = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
const CharType* ft_string_view<CharType>::data() const
{
    const CharType *pointer_value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::data");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ft_nullptr);
    }
    pointer_value = this->_data;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (pointer_value);
}

template <typename CharType>
ft_size_t ft_string_view<CharType>::size() const
{
    ft_size_t size_value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    size_value = this->_size;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (size_value);
}

template <typename CharType>
ft_bool ft_string_view<CharType>::empty() const
{
    ft_bool is_empty_value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::empty");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_TRUE);
    }
    is_empty_value = (this->_size == 0);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (is_empty_value);
}

template <typename CharType>
typename ft_string_view<CharType>::char_proxy ft_string_view<CharType>::operator[](
    ft_size_t index) const
{
    CharType char_value;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::operator[]");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (char_proxy(CharType(), set_error(lock_error)));
    if (index >= this->_size || this->_data == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        return (char_proxy(CharType(), set_error(FT_ERR_INVALID_ARGUMENT)));
    }
    char_value = this->_data[index];
    (void)this->unlock_internal(lock_acquired);
    return (char_proxy(char_value, set_error(FT_ERR_SUCCESS)));
}

template <typename CharType>
int32_t ft_string_view<CharType>::compare(const ft_string_view& other) const
{
    ft_size_t first_index;
    ft_size_t first_size;
    ft_size_t second_size;
    const CharType *first_data;
    const CharType *second_data;
    int32_t compare_result;
    ft_bool first_lock_acquired;
    ft_bool second_lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::compare");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_string_view::compare other");
    first_lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&first_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    second_lock_acquired = FT_FALSE;
    lock_error = other.lock_internal(&second_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->unlock_internal(first_lock_acquired);
        return (set_error(lock_error));
    }
    first_data = this->_data;
    second_data = other._data;
    first_size = this->_size;
    second_size = other._size;
    ft_size_t common_size = first_size;
    if (second_size < common_size)
        common_size = second_size;
    first_index = 0;
    compare_result = 0;
    if constexpr (std::is_same_v<CharType, char> || std::is_same_v<CharType, signed char>
        || std::is_same_v<CharType, unsigned char>)
    {
        constexpr ft_size_t BLOCK_SIZE = 256U;
        // Skip equal blocks. Ordering still follows CharType at the first difference.
        while (common_size - first_index >= BLOCK_SIZE
            && ft_memcmp(first_data + first_index, second_data + first_index, BLOCK_SIZE) == 0)
            first_index += BLOCK_SIZE;
    }
    while (first_index < common_size)
    {
        if (first_data[first_index] != second_data[first_index])
        {
            if (first_data[first_index] < second_data[first_index])
                compare_result = -1;
            else
                compare_result = 1;
            break ;
        }
        ++first_index;
    }
    if (compare_result == 0)
    {
        if (first_size < second_size)
            compare_result = -1;
        else if (first_size > second_size)
            compare_result = 1;
    }
    (void)other.unlock_internal(second_lock_acquired);
    (void)this->unlock_internal(first_lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (compare_result);
}

template <typename CharType>
int32_t ft_string_view<CharType>::substr(ft_size_t position, ft_size_t count,
    ft_string_view& out_view) const
{
    const CharType *new_data;
    ft_size_t new_count;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::substr");
    errno_abort_if_uninitialised_or_destroyed(out_view._initialised_state, "ft_string_view::substr out_view");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    if (position > this->_size)
    {
        (void)this->unlock_internal(lock_acquired);
        return (set_error(FT_ERR_INVALID_ARGUMENT));
    }
    new_count = this->_size - position;
    if (count != npos && count < new_count)
        new_count = count;
    new_data = this->_data + position;
    out_view._data = new_data;
    out_view._size = new_count;
    (void)this->unlock_internal(lock_acquired);
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
int32_t ft_string_view<CharType>::substr(ft_size_t position,
    ft_string_view& out_view) const
{
    return (set_error(this->substr(position, npos, out_view)));
}

template <typename CharType>
int32_t ft_string_view<CharType>::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (set_error(initialize_result));
    }
    this->_mutex = new_mutex;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
int32_t ft_string_view<CharType>::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (set_error(FT_ERR_INVALID_STATE));
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (set_error(destroy_result));
    return (set_error(FT_ERR_SUCCESS));
}

template <typename CharType>
ft_bool ft_string_view<CharType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename CharType>
int32_t ft_string_view<CharType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_string_view::lock");
    lock_result = this->lock_internal(lock_acquired);
    return (set_error(lock_result));
}

template <typename CharType>
void ft_string_view<CharType>::unlock(ft_bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename CharType>
int32_t ft_string_view<CharType>::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_string_view::get_error");
    return (_last_error);
}

template <typename CharType>
const char *ft_string_view<CharType>::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_string_view::get_error_str");
    return (ft_strerror(_last_error));
}

template <typename CharType>
const ft_size_t ft_string_view<CharType>::npos = static_cast<ft_size_t>(-1);

template <typename CharType>
thread_local int32_t ft_string_view<CharType>::_last_error = FT_ERR_SUCCESS;

#endif
