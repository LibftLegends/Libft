#ifndef FT_MATRIX_HPP
#define FT_MATRIX_HPP

#include "constructor.hpp"
#include "move.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <type_traits>

template <typename ElementType>
class ft_matrix
{
    private:
        ElementType               *_data;
        ft_size_t                  _configured_rows;
        ft_size_t                  _configured_cols;
        ft_size_t                  _rows;
        ft_size_t                  _cols;
        mutable pt_recursive_mutex *_mutex;
        uint8_t                    _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        int32_t unlock_internal(ft_bool lock_acquired) const;
        int32_t lock_operands(const ft_matrix &other, ft_bool &this_locked,
            ft_bool &other_locked) const;
        static ft_matrix *create_result(ft_size_t rows, ft_size_t cols);
        void clear_unlocked();
        ft_bool init_unlocked(ft_size_t rows, ft_size_t cols);

    public:
        ft_matrix(ft_size_t rows = 0, ft_size_t cols = 0);
        ft_matrix(const ft_matrix<ElementType> &other) = delete;
        ~ft_matrix();

        ft_matrix& operator=(const ft_matrix&) = delete;
        ft_matrix(ft_matrix&& other) noexcept = delete;
        ft_matrix& operator=(ft_matrix&& other) noexcept = delete;

        int32_t initialize();
        int32_t initialize(ft_size_t rows, ft_size_t cols);
        int32_t destroy();
        int32_t move(ft_matrix<ElementType> &other);

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        ft_bool init(ft_size_t rows, ft_size_t cols);

        ElementType& at(ft_size_t row_index, ft_size_t column_index);
        const ElementType& at(ft_size_t row_index, ft_size_t column_index) const;
        ft_size_t rows() const;
        ft_size_t cols() const;

        ft_matrix *add(const ft_matrix& other) const;
        ft_matrix *multiply(const ft_matrix& other) const;
        ft_matrix *transpose() const;
        ElementType determinant() const;

        void clear();
        int32_t get_error() const;
        const char *get_error_str() const;
};

template <typename ElementType>
thread_local int32_t ft_matrix<ElementType>::_last_error = FT_ERR_SUCCESS;

template <typename ElementType>
int32_t ft_matrix<ElementType>::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::lock_internal(ft_bool *lock_acquired) const
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

template <typename ElementType>
int32_t ft_matrix<ElementType>::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_matrix<ElementType>::clear_unlocked()
{
    ft_size_t index;
    ft_size_t total;

    if (this->_data != ft_nullptr)
    {
        total = this->_rows * this->_cols;
        index = 0;
        while (index < total)
        {
            destroy_at(&this->_data[index]);
            index = index + 1;
        }
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    this->_rows = 0;
    this->_cols = 0;
    return ;
}

template <typename ElementType>
ft_bool ft_matrix<ElementType>::init_unlocked(ft_size_t rows, ft_size_t cols)
{
    ft_size_t total;
    ft_size_t index;

    constexpr ft_size_t MAXIMUM_ELEMENTS = FT_SYSTEM_SIZE_MAX / sizeof(ElementType);
    if (cols != 0U && rows > MAXIMUM_ELEMENTS / cols)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    this->clear_unlocked();
    total = rows * cols;
    if (total == 0)
    {
        this->_rows = rows;
        this->_cols = cols;
        return (FT_TRUE);
    }
    this->_data = static_cast<ElementType *>(cma_malloc(sizeof(ElementType)
                * total));
    if (this->_data == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (FT_FALSE);
    }
    index = 0;
    while (index < total)
    {
        construct_default_at(&this->_data[index]);
        index = index + 1;
    }
    this->_rows = rows;
    this->_cols = cols;
    return (FT_TRUE);
}

template <typename ElementType>
ft_matrix<ElementType>::ft_matrix(ft_size_t rows, ft_size_t cols)
    : _data(ft_nullptr)
    , _configured_rows(rows)
    , _configured_cols(cols)
    , _rows(0)
    , _cols(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename ElementType>
ft_matrix<ElementType>::~ft_matrix()
{
    uint32_t previous_error;

    previous_error = ft_matrix<ElementType>::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->_mutex != ft_nullptr)
        (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_matrix::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_data = ft_nullptr;
    this->_rows = 0;
    this->_cols = 0;
    if (this->init_unlocked(this->_configured_rows, this->_configured_cols)
        == FT_FALSE)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_NO_MEMORY));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::initialize(ft_size_t rows, ft_size_t cols)
{
    this->_configured_rows = rows;
    this->_configured_cols = cols;
    return (set_error(this->initialize()));
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::destroy()
{
    int32_t first_error;
    int32_t disable_error;

    first_error = FT_ERR_SUCCESS;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->clear_unlocked();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::move(ft_matrix<ElementType> &other)
{
    int32_t destroy_result;

    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "ft_matrix::move",
            "source object is uninitialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (set_error(destroy_result));
    }
    this->_configured_rows = other._configured_rows;
    this->_configured_cols = other._configured_cols;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_data = ft_nullptr;
        this->_rows = 0;
        this->_cols = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_data = other._data;
    this->_rows = other._rows;
    this->_cols = other._cols;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._rows = 0;
    other._cols = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::disable_thread_safety()
{
    int32_t destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

template <typename ElementType>
ft_bool ft_matrix<ElementType>::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::lock(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::lock");
    lock_error = this->lock_internal(lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    return (set_error(FT_ERR_SUCCESS));
}

template <typename ElementType>
void ft_matrix<ElementType>::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
ft_bool ft_matrix<ElementType>::init(ft_size_t rows, ft_size_t cols)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool init_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::init");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (FT_FALSE);
    }
    init_result = this->init_unlocked(rows, cols);
    (void)this->unlock_internal(lock_acquired);
    if (init_result == FT_FALSE)
        return (FT_FALSE);
    set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

template <typename ElementType>
ElementType& ft_matrix<ElementType>::at(ft_size_t row_index, ft_size_t column_index)
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType *element_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::at");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (row_index >= this->_rows || column_index >= this->_cols
        || this->_data == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_INVALID_ARGUMENT);
        return (error_element);
    }
    element_pointer = &this->_data[row_index * this->_cols + column_index];
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*element_pointer);
}

template <typename ElementType>
const ElementType& ft_matrix<ElementType>::at(ft_size_t row_index,
    ft_size_t column_index) const
{
    static ElementType error_element = ElementType();
    ft_bool lock_acquired;
    int32_t lock_error;
    const ElementType *element_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::at const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (error_element);
    }
    if (row_index >= this->_rows || column_index >= this->_cols
        || this->_data == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_INVALID_ARGUMENT);
        return (error_element);
    }
    element_pointer = &this->_data[row_index * this->_cols + column_index];
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (*element_pointer);
}

template <typename ElementType>
ft_size_t ft_matrix<ElementType>::rows() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t current_rows;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::rows");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    current_rows = this->_rows;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (current_rows);
}

template <typename ElementType>
ft_size_t ft_matrix<ElementType>::cols() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t current_cols;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::cols");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (0);
    }
    current_cols = this->_cols;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (current_cols);
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::lock_operands(const ft_matrix &other,
    ft_bool &this_locked, ft_bool &other_locked) const
{
    this_locked = FT_FALSE;
    other_locked = FT_FALSE;
    if (this == &other)
        return (this->lock_internal(&this_locked));
    const ft_matrix *first = this;
    const ft_matrix *second = &other;
    ft_bool *first_locked = &this_locked;
    ft_bool *second_locked = &other_locked;
    if (std::less<const ft_matrix *>{}(&other, this))
    {
        first = &other;
        second = this;
        first_locked = &other_locked;
        second_locked = &this_locked;
    }
    int32_t error = first->lock_internal(first_locked);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = second->lock_internal(second_locked);
    if (error != FT_ERR_SUCCESS)
        (void)first->unlock_internal(*first_locked);
    return (error);
}

template <typename ElementType>
ft_matrix<ElementType> *ft_matrix<ElementType>::create_result(ft_size_t rows, ft_size_t cols)
{
    ft_matrix *result = new (std::nothrow) ft_matrix(rows, cols);
    if (result == ft_nullptr)
    {
        set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    const int32_t error = result->initialize();
    if (error != FT_ERR_SUCCESS)
    {
        delete result;
        set_error(error);
        return (ft_nullptr);
    }
    return (result);
}

template <typename ElementType>
ft_matrix<ElementType> *ft_matrix<ElementType>::add(const ft_matrix& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::add");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_matrix::add(other)");
    ft_bool this_locked;
    ft_bool other_locked;
    const int32_t lock_error = this->lock_operands(other, this_locked, other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ft_nullptr);
    }
    ft_matrix *result = ft_nullptr;
    if (this->_rows != other._rows || this->_cols != other._cols)
        set_error(FT_ERR_INVALID_ARGUMENT);
    else
        result = create_result(this->_rows, this->_cols);
    if (result != ft_nullptr)
    {
        const ft_size_t total = this->_rows * this->_cols;
        ft_size_t index = 0;
        while (index < total)
        {
            result->_data[index] = this->_data[index] + other._data[index];
            ++index;
        }
    }
    (void)other.unlock_internal(other_locked);
    (void)this->unlock_internal(this_locked);
    if (result != ft_nullptr)
        set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename ElementType>
ft_matrix<ElementType> *ft_matrix<ElementType>::multiply(const ft_matrix& other) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::multiply");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "ft_matrix::multiply(other)");
    ft_bool this_locked;
    ft_bool other_locked;
    const int32_t lock_error = this->lock_operands(other, this_locked, other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ft_nullptr);
    }
    ft_matrix *result = ft_nullptr;
    if (this->_cols != other._rows)
        set_error(FT_ERR_INVALID_ARGUMENT);
    else
        result = create_result(this->_rows, other._cols);
    if (result != ft_nullptr && result->_data != ft_nullptr)
    {
        ft_size_t row_index = 0;
        while (row_index < this->_rows)
        {
            ElementType *result_row = result->_data + row_index * other._cols;
            if constexpr (std::is_arithmetic_v<ElementType>)
            {
                // Process contiguous right-hand and output rows in inner-index order.
                ft_size_t inner_index = 0;
                while (inner_index < this->_cols)
                {
                    const ElementType left_value = this->_data[row_index * this->_cols + inner_index];
                    const ElementType *right_row = other._data + inner_index * other._cols;
                    ft_size_t column_index = 0;
                    while (column_index < other._cols)
                    {
                        result_row[column_index] = result_row[column_index]
                            + left_value * right_row[column_index];
                        ++column_index;
                    }
                    ++inner_index;
                }
            }
            else
            {
                ft_size_t column_index = 0;
                while (column_index < other._cols)
                {
                    ElementType sum_value;
                    sum_value = ElementType();
                    ft_size_t inner_index = 0;
                    while (inner_index < this->_cols)
                    {
                        sum_value = sum_value + this->_data[row_index * this->_cols + inner_index]
                            * other._data[inner_index * other._cols + column_index];
                        ++inner_index;
                    }
                    result_row[column_index] = sum_value;
                    ++column_index;
                }
            }
            ++row_index;
        }
    }
    (void)other.unlock_internal(other_locked);
    (void)this->unlock_internal(this_locked);
    if (result != ft_nullptr)
        set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename ElementType>
ft_matrix<ElementType> *ft_matrix<ElementType>::transpose() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::transpose");
    ft_bool lock_acquired;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (ft_nullptr);
    }
    ft_matrix *result = create_result(this->_cols, this->_rows);
    if (result != ft_nullptr && result->_data != ft_nullptr)
    {
        ft_size_t row_index = 0;
        while (row_index < this->_rows)
        {
            ft_size_t column_index = 0;
            while (column_index < this->_cols)
            {
                result->_data[column_index * this->_rows + row_index] =
                    this->_data[row_index * this->_cols + column_index];
                ++column_index;
            }
            ++row_index;
        }
    }
    (void)this->unlock_internal(lock_acquired);
    if (result != ft_nullptr)
        set_error(FT_ERR_SUCCESS);
    return (result);
}

template <typename ElementType>
ElementType ft_matrix<ElementType>::determinant() const
{
    ElementType determinant_value;
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t dimension;
    ft_size_t size;
    ElementType *temp;
    ft_size_t index;
    ft_size_t pivot_row;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::determinant");
    determinant_value = ElementType();
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (determinant_value);
    }
    if (this->_rows != this->_cols)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_INVALID_ARGUMENT);
        return (determinant_value);
    }
    dimension = this->_rows;
    size = dimension * dimension;
    temp = static_cast<ElementType *>(cma_malloc(sizeof(ElementType) * size));
    if (temp == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        set_error(FT_ERR_NO_MEMORY);
        return (determinant_value);
    }
    index = 0;
    while (index < size)
    {
        temp[index] = this->_data[index];
        index = index + 1;
    }
    determinant_value = determinant_value + static_cast<ElementType>(1);
    pivot_row = 0;
    while (pivot_row < dimension)
    {
        ft_size_t pivot_index;

        pivot_index = pivot_row;
        while (pivot_index < dimension
            && temp[pivot_index * dimension + pivot_row] == ElementType())
        {
            pivot_index = pivot_index + 1;
        }
        if (pivot_index == dimension)
        {
            determinant_value = ElementType();
            break ;
        }
        if (pivot_index != pivot_row)
        {
            ft_size_t swap_index;

            swap_index = 0;
            while (swap_index < dimension)
            {
                ElementType swap_value;

                swap_value = temp[pivot_row * dimension + swap_index];
                temp[pivot_row * dimension + swap_index] =
                    temp[pivot_index * dimension + swap_index];
                temp[pivot_index * dimension + swap_index] = swap_value;
                swap_index = swap_index + 1;
            }
            determinant_value = determinant_value * static_cast<ElementType>(-1);
        }
        ElementType pivot_value;
        ft_size_t row_index;

        pivot_value = temp[pivot_row * dimension + pivot_row];
        determinant_value = determinant_value * pivot_value;
        row_index = pivot_row + 1;
        while (row_index < dimension)
        {
            ElementType factor_value;
            ft_size_t column_index;

            factor_value = temp[row_index * dimension + pivot_row] / pivot_value;
            column_index = pivot_row;
            while (column_index < dimension)
            {
                temp[row_index * dimension + column_index] =
                    temp[row_index * dimension + column_index]
                    - factor_value
                    * temp[pivot_row * dimension + column_index];
                column_index = column_index + 1;
            }
            row_index = row_index + 1;
        }
        pivot_row = pivot_row + 1;
    }
    cma_free(temp);
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (determinant_value);
}

template <typename ElementType>
void ft_matrix<ElementType>::clear()
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_matrix::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    this->clear_unlocked();
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename ElementType>
int32_t ft_matrix<ElementType>::get_error() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_matrix::get_error");
    return (ft_matrix<ElementType>::_last_error);
}

template <typename ElementType>
const char *ft_matrix<ElementType>::get_error_str() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_matrix::get_error_str");
    return (ft_strerror(this->get_error()));
}

#endif
