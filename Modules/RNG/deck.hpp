#ifndef RNG_DECK_HPP
#define RNG_DECK_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Template/vector.hpp"
#include "../Template/swap.hpp"
#include "../Template/pair.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "rng.hpp"
#include <climits>
#include <new>

template<typename ElementType>
class ft_deck : public ft_vector<ElementType*>
{
    private:
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

        int32_t lock_deck(ft_bool *lock_acquired) const;
        void unlock_deck(ft_bool lock_acquired) const;

    public:
        int32_t initialize() noexcept;
        int32_t initialize(const ft_deck &other) noexcept;
        int32_t initialize(ft_deck &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(ft_deck &other) noexcept;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        ft_deck() noexcept;
        ft_deck(const ft_deck &other) noexcept = delete;
        ft_deck(ft_deck &&other) noexcept = delete;
        ~ft_deck() noexcept;

        ft_deck &operator=(const ft_deck&) = delete;
        ft_deck &operator=(ft_deck&&) = delete;

        Pair<int32_t,  ElementType *> pop_random_element() noexcept;
        Pair<int32_t,  ElementType *> get_random_element() const noexcept;
        int32_t shuffle() noexcept;
        Pair<int32_t,  ElementType *> draw_top_element() noexcept;
        Pair<int32_t,  ElementType *> peek_top_element() const noexcept;
};

template<typename ElementType>
ft_deck<ElementType>::ft_deck() noexcept
    : ft_vector<ElementType*>()
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template<typename ElementType>
ft_deck<ElementType>::~ft_deck() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

template<typename ElementType>
int32_t ft_deck<ElementType>::initialize() noexcept
{
    int32_t vector_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_deck::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    vector_error = this->ft_vector<ElementType*>::initialize();
    if (vector_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_deck<ElementType>::initialize(const ft_deck &other) noexcept
{
    int32_t vector_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_deck::initialize(const ft_deck &) source", "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        vector_error = this->destroy();
        if (vector_error != FT_ERR_SUCCESS)
            return (vector_error);
    }
    vector_error = this->initialize();
    if (vector_error != FT_ERR_SUCCESS)
        return (vector_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (this->destroy());
    vector_error = this->ft_vector<ElementType*>::copy_from(other);
    if (vector_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (vector_error);
    }
    if (other._mutex != ft_nullptr)
    {
        vector_error = this->enable_thread_safety();
        if (vector_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (vector_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_deck<ElementType>::initialize(ft_deck &&other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_deck::initialize(ft_deck &&) source", "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    ft_bool other_lock_acquired = FT_FALSE;
    const int32_t lock_error = other.lock_deck(&other_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        if (this->ft_vector<ElementType*>::is_initialised() == FT_CLASS_STATE_UNINITIALISED)
        {
            (void)this->ft_vector<ElementType*>::initialize();
            (void)this->ft_vector<ElementType*>::destroy();
        }
        return (lock_error);
    }
    const int32_t vector_error = this->ft_vector<ElementType*>::move(other);
    other.unlock_deck(other_lock_acquired);
    if (vector_error != FT_ERR_SUCCESS)
        return (vector_error);
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_initialised_state = other._initialised_state;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_deck<ElementType>::destroy() noexcept
{
    int32_t thread_error;
    int32_t vector_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    thread_error = this->disable_thread_safety();
    vector_error = this->ft_vector<ElementType*>::destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (thread_error != FT_ERR_SUCCESS)
        return (thread_error);
    return (vector_error);
}

template<typename ElementType>
uint32_t ft_deck<ElementType>::move(ft_deck &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(static_cast<ft_deck &&>(other))));
}

template<typename ElementType>
int32_t ft_deck<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t base_result;
    int32_t mutex_error;

    base_result = this->ft_vector<ElementType*>::enable_thread_safety();
    if (base_result != FT_ERR_SUCCESS)
        return (base_result);
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_deck<ElementType>::disable_thread_safety()
{
    int32_t destroy_error;
    int32_t base_error;

    destroy_error = FT_ERR_SUCCESS;
    if (this->_mutex != ft_nullptr)
    {
        destroy_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
    }
    base_error = this->ft_vector<ElementType*>::disable_thread_safety();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (base_error);
}

template<typename ElementType>
ft_bool ft_deck<ElementType>::is_thread_safe() const
{
    return (this->_mutex != ft_nullptr);
}

template<typename ElementType>
int32_t ft_deck<ElementType>::lock_deck(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
void ft_deck<ElementType>::unlock_deck(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

template<typename ElementType>
Pair<int32_t,  ElementType *> ft_deck<ElementType>::pop_random_element() noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;
    ft_size_t deck_size;
    int32_t roll_result;
    ft_size_t index;
    ElementType *element;

    element = ft_nullptr;
    lock_acquired = FT_FALSE;
    operation_error = this->lock_deck(&lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
    {
        deck_size = this->size();
        operation_error = this->get_error();
    }
    if (operation_error == FT_ERR_SUCCESS && deck_size == 0)
        operation_error = FT_ERR_EMPTY;
    if (operation_error == FT_ERR_SUCCESS && deck_size > static_cast<ft_size_t>(INT_MAX))
        operation_error = FT_ERR_OUT_OF_RANGE;
    if (operation_error == FT_ERR_SUCCESS)
    {
        roll_result = ft_dice_roll(1, static_cast<int32_t>(deck_size));
        if (roll_result < 1)
            operation_error = FT_ERR_OUT_OF_RANGE;
    }
    if (operation_error == FT_ERR_SUCCESS)
    {
        index = static_cast<ft_size_t>(roll_result - 1);
        element = this->release_at(index);
        operation_error = this->get_error();
    }
    this->unlock_deck(lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  ElementType *>(operation_error, ft_nullptr));
    return (Pair<int32_t,  ElementType *>(FT_ERR_SUCCESS, element));
}

template<typename ElementType>
Pair<int32_t,  ElementType *> ft_deck<ElementType>::get_random_element() const noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;
    ft_size_t deck_size;
    int32_t roll_result;
    ft_size_t index;
    ElementType *element;

    element = ft_nullptr;
    lock_acquired = FT_FALSE;
    operation_error = this->lock_deck(&lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
    {
        deck_size = this->size();
        operation_error = this->get_error();
    }
    if (operation_error == FT_ERR_SUCCESS && deck_size == 0)
        operation_error = FT_ERR_EMPTY;
    if (operation_error == FT_ERR_SUCCESS && deck_size > static_cast<ft_size_t>(INT_MAX))
        operation_error = FT_ERR_OUT_OF_RANGE;
    if (operation_error == FT_ERR_SUCCESS)
    {
        roll_result = ft_dice_roll(1, static_cast<int32_t>(deck_size));
        if (roll_result < 1)
            operation_error = FT_ERR_OUT_OF_RANGE;
    }
    if (operation_error == FT_ERR_SUCCESS)
    {
        index = static_cast<ft_size_t>(roll_result - 1);
        element = (*this)[index];
        operation_error = this->get_error();
    }
    this->unlock_deck(lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  ElementType *>(operation_error, ft_nullptr));
    return (Pair<int32_t,  ElementType *>(FT_ERR_SUCCESS, element));
}

template<typename ElementType>
Pair<int32_t,  ElementType *> ft_deck<ElementType>::draw_top_element() noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;
    ft_size_t deck_size;
    ft_size_t index;
    ElementType *element;

    element = ft_nullptr;
    lock_acquired = FT_FALSE;
    operation_error = this->lock_deck(&lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
    {
        deck_size = this->size();
        operation_error = this->get_error();
    }
    if (operation_error == FT_ERR_SUCCESS && deck_size == 0)
        operation_error = FT_ERR_EMPTY;
    if (operation_error == FT_ERR_SUCCESS && deck_size > static_cast<ft_size_t>(INT_MAX))
        operation_error = FT_ERR_OUT_OF_RANGE;
    if (operation_error == FT_ERR_SUCCESS)
    {
        index = deck_size - 1;
        element = this->release_at(index);
        operation_error = this->get_error();
    }
    this->unlock_deck(lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  ElementType *>(operation_error, ft_nullptr));
    return (Pair<int32_t,  ElementType *>(FT_ERR_SUCCESS, element));
}

template<typename ElementType>
Pair<int32_t,  ElementType *> ft_deck<ElementType>::peek_top_element() const noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;
    ft_size_t deck_size;
    ft_size_t index;
    ElementType *element;

    element = ft_nullptr;
    lock_acquired = FT_FALSE;
    operation_error = this->lock_deck(&lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
    {
        deck_size = this->size();
        operation_error = this->get_error();
    }
    if (operation_error == FT_ERR_SUCCESS && deck_size == 0)
        operation_error = FT_ERR_EMPTY;
    if (operation_error == FT_ERR_SUCCESS && deck_size > static_cast<ft_size_t>(INT_MAX))
        operation_error = FT_ERR_OUT_OF_RANGE;
    if (operation_error == FT_ERR_SUCCESS)
    {
        index = deck_size - 1;
        element = (*this)[index];
        operation_error = this->get_error();
    }
    this->unlock_deck(lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  ElementType *>(operation_error, ft_nullptr));
    return (Pair<int32_t,  ElementType *>(FT_ERR_SUCCESS, element));
}

template<typename ElementType>
int32_t ft_deck<ElementType>::shuffle() noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    ft_bool vector_lock_acquired = FT_FALSE;
    ft_size_t deck_size = 0U;
    int32_t operation_error = this->lock_deck(&lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
        operation_error = this->lock(&vector_lock_acquired);
    if (operation_error == FT_ERR_SUCCESS)
    {
        deck_size = this->size();
        operation_error = this->get_error();
    }
    if (operation_error == FT_ERR_SUCCESS && deck_size == 0)
        operation_error = FT_ERR_EMPTY;
    if (operation_error == FT_ERR_SUCCESS && deck_size > static_cast<ft_size_t>(INT_MAX))
        operation_error = FT_ERR_OUT_OF_RANGE;
    if (operation_error == FT_ERR_SUCCESS && deck_size > 1U)
    {
        ElementType **elements = this->begin();
        operation_error = this->get_error();
        constexpr ft_size_t BATCH_CAPACITY = 64U;
        int32_t draws[BATCH_CAPACITY];
        ft_size_t index = deck_size - 1U;
        while (operation_error == FT_ERR_SUCCESS && index > 0U)
        {
            ft_size_t count = index;
            if (count > BATCH_CAPACITY)
                count = BATCH_CAPACITY;
            const ft_bool sampled = (ft_random_int_vector(0, INT32_MAX, draws, count)
                == FT_ERR_SUCCESS);
            ft_size_t draw_index = 0U;
            while (draw_index < count)
            {
                int32_t draw;
                if (sampled != FT_FALSE)
                    draw = draws[draw_index];
                else
                    draw = ft_random_int();
                const ft_size_t random_index = static_cast<ft_size_t>(
                    draw % static_cast<int32_t>(index + 1U));
                ft_swap(elements[index], elements[random_index]);
                index -= 1U;
                draw_index += 1U;
            }
        }
    }
    if (vector_lock_acquired != FT_FALSE)
        this->unlock(vector_lock_acquired);
    this->unlock_deck(lock_acquired);
    return (operation_error);
}

#endif
