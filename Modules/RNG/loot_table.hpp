#ifndef RNG_LOOT_TABLE_HPP
#define RNG_LOOT_TABLE_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Template/pair.hpp"
#include "rng.hpp"
#include <new>

template<typename ElementType>
class ft_loot_entry
{
    private:
        ElementType *_item;
        int32_t _weight;
        int32_t _rarity;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

        int32_t lock_entry(ft_bool *lock_acquired) const noexcept;
        void unlock_entry(ft_bool lock_acquired) const noexcept;

    public:
        ft_loot_entry() noexcept;
        ft_loot_entry(const ft_loot_entry &other) noexcept = delete;
        ft_loot_entry(ft_loot_entry &&other) noexcept = delete;
        ~ft_loot_entry() noexcept;

        ft_loot_entry &operator=(const ft_loot_entry &other) = delete;
        ft_loot_entry &operator=(ft_loot_entry &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_loot_entry &other) noexcept;
        int32_t initialize(ft_loot_entry &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(ft_loot_entry &other) noexcept;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t set_item(ElementType *item) noexcept;
        Pair<int32_t,  ElementType *> get_item() const noexcept;

        int32_t set_weight(int32_t weight) noexcept;
        Pair<int32_t,  int32_t> get_weight() const noexcept;

        int32_t set_rarity(int32_t rarity) noexcept;
        Pair<int32_t,  int32_t> get_rarity() const noexcept;

        Pair<int32_t,  int32_t> get_effective_weight() const noexcept;

#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

template<typename ElementType>
class ft_loot_table
{
    private:
        struct entry
        {
            ElementType *item;
            uint32_t cumulative_weight;
        };

        // Oversized tables still accept entries, but cannot be drawn from.
        static constexpr uint32_t OVERFLOW_WEIGHT = static_cast<uint32_t>(INT32_MAX) + 1U;
        entry *_entries;
        ft_size_t _size;
        ft_size_t _capacity;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

        int32_t lock_table(ft_bool *lock_acquired) const noexcept;
        void unlock_table(ft_bool lock_acquired) const noexcept;
        int32_t reserve_capacity(ft_size_t requested_capacity);
        int32_t select_entry_locked(ft_size_t &selected_index) const noexcept;

    public:
        ft_loot_table() noexcept;
        ft_loot_table(const ft_loot_table &other) noexcept = delete;
        ft_loot_table(ft_loot_table &&other) noexcept = delete;
        ~ft_loot_table() noexcept;

        ft_loot_table &operator=(const ft_loot_table &other) = delete;
        ft_loot_table &operator=(ft_loot_table &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_loot_table &other) noexcept;
        int32_t initialize(ft_loot_table &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(ft_loot_table &other) noexcept;

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t add_element(ElementType *element_pointer, int32_t weight, int32_t rarity);
        Pair<int32_t,  ElementType *> get_random_loot() const;
        Pair<int32_t,  ElementType *> pop_random_loot();
        ft_size_t size() const noexcept;

#ifdef LIBFT_TEST_BUILD
        pt_recursive_mutex *get_mutex_for_validation() const noexcept;
#endif
};

static inline int32_t rng_loot_effective_weight(int32_t weight, int32_t rarity) noexcept
{
    if (rarity >= weight)
        return (1);
    return (weight / (rarity + 1));
}

template<typename ElementType>
ft_loot_entry<ElementType>::ft_loot_entry() noexcept
    : _item(ft_nullptr)
    , _weight(1)
    , _rarity(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template<typename ElementType>
ft_loot_entry<ElementType>::~ft_loot_entry() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_loot_entry::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_item = ft_nullptr;
    this->_weight = 1;
    this->_rarity = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::initialize(const ft_loot_entry &other) noexcept
{
    int32_t operation_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_loot_entry::initialize(const ft_loot_entry &) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
            return (operation_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    this->_item = other._item;
    this->_weight = other._weight;
    this->_rarity = other._rarity;
    if (other._mutex != ft_nullptr)
    {
        operation_error = this->enable_thread_safety();
        if (operation_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (operation_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::initialize(ft_loot_entry &&other) noexcept
{
    int32_t operation_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_loot_entry::initialize(ft_loot_entry &&) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
            return (operation_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    if (other._mutex != ft_nullptr)
    {
        operation_error = this->enable_thread_safety();
        if (operation_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (operation_error);
        }
    }
    this->_item = other._item;
    this->_weight = other._weight;
    this->_rarity = other._rarity;
    other._item = ft_nullptr;
    other._weight = 1;
    other._rarity = 0;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    destroy_error = this->disable_thread_safety();
    this->_item = ft_nullptr;
    this->_weight = 1;
    this->_rarity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (destroy_error);
}

template<typename ElementType>
uint32_t ft_loot_entry<ElementType>::move(ft_loot_entry &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(
            static_cast<ft_loot_entry &&>(other))));
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::disable_thread_safety()
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

template<typename ElementType>
ft_bool ft_loot_entry<ElementType>::is_thread_safe() const
{
    return (this->_mutex != ft_nullptr);
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::lock_entry(ft_bool *lock_acquired) const noexcept
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
void ft_loot_entry<ElementType>::unlock_entry(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::set_item(ElementType *item) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_item = item;
    this->unlock_entry(lock_acquired);
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
Pair<int32_t,  ElementType *> ft_loot_entry<ElementType>::get_item() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ElementType *item_pointer;

    lock_acquired = FT_FALSE;
    item_pointer = ft_nullptr;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  ElementType *>(lock_error, item_pointer));
    item_pointer = this->_item;
    this->unlock_entry(lock_acquired);
    return (Pair<int32_t,  ElementType *>(FT_ERR_SUCCESS, item_pointer));
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::set_weight(int32_t weight) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (weight <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_weight = weight;
    this->unlock_entry(lock_acquired);
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
Pair<int32_t,  int32_t> ft_loot_entry<ElementType>::get_weight() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t weight_value;

    lock_acquired = FT_FALSE;
    weight_value = 0;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  int32_t>(lock_error, weight_value));
    weight_value = this->_weight;
    this->unlock_entry(lock_acquired);
    return (Pair<int32_t,  int32_t>(FT_ERR_SUCCESS, weight_value));
}

template<typename ElementType>
int32_t ft_loot_entry<ElementType>::set_rarity(int32_t rarity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (rarity < 0)
        rarity = 0;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_rarity = rarity;
    this->unlock_entry(lock_acquired);
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
Pair<int32_t,  int32_t> ft_loot_entry<ElementType>::get_rarity() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t rarity_value;

    lock_acquired = FT_FALSE;
    rarity_value = 0;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  int32_t>(lock_error, rarity_value));
    rarity_value = this->_rarity;
    this->unlock_entry(lock_acquired);
    return (Pair<int32_t,  int32_t>(FT_ERR_SUCCESS, rarity_value));
}

template<typename ElementType>
Pair<int32_t,  int32_t> ft_loot_entry<ElementType>::get_effective_weight() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t effective_weight;

    lock_acquired = FT_FALSE;
    effective_weight = 0;
    lock_error = this->lock_entry(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t,  int32_t>(lock_error, effective_weight));
    effective_weight = rng_loot_effective_weight(this->_weight, this->_rarity);
    this->unlock_entry(lock_acquired);
    return (Pair<int32_t,  int32_t>(FT_ERR_SUCCESS, effective_weight));
}

#ifdef LIBFT_TEST_BUILD
template<typename ElementType>
pt_recursive_mutex *ft_loot_entry<ElementType>::get_mutex_for_validation() const noexcept
{
    return (this->_mutex);
}
#endif

template<typename ElementType>
ft_loot_table<ElementType>::ft_loot_table() noexcept
    : _entries(ft_nullptr)
    , _size(0)
    , _capacity(0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template<typename ElementType>
ft_loot_table<ElementType>::~ft_loot_table() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_loot_table::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_entries = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::initialize(const ft_loot_table &other) noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_loot_table::initialize(const ft_loot_table &) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
            return (operation_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    lock_acquired = FT_FALSE;
    operation_error = other.lock_table(&lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (operation_error);
    }
    operation_error = this->reserve_capacity(other._size);
    if (operation_error == FT_ERR_SUCCESS && other._mutex != ft_nullptr)
        operation_error = this->enable_thread_safety();
    if (operation_error == FT_ERR_SUCCESS)
    {
        if (other._size != 0U)
            ft_memcpy(this->_entries, other._entries, other._size * sizeof(entry));
        this->_size = other._size;
    }
    other.unlock_table(lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (operation_error);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::initialize(ft_loot_table &&other) noexcept
{
    ft_bool lock_acquired;
    int32_t operation_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "ft_loot_table::initialize(ft_loot_table &&) source",
            "source is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        operation_error = this->destroy();
        if (operation_error != FT_ERR_SUCCESS)
            return (operation_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    operation_error = this->initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    lock_acquired = FT_FALSE;
    operation_error = other.lock_table(&lock_acquired);
    if (operation_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (operation_error);
    }
    if (other._mutex != ft_nullptr)
    {
        operation_error = this->enable_thread_safety();
        if (operation_error != FT_ERR_SUCCESS)
        {
            other.unlock_table(lock_acquired);
            (void)this->destroy();
            return (operation_error);
        }
    }
    this->_entries = other._entries;
    this->_size = other._size;
    this->_capacity = other._capacity;
    other._entries = ft_nullptr;
    other._size = 0;
    other._capacity = 0;
    other.unlock_table(lock_acquired);
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::destroy() noexcept
{
    int32_t thread_error;
    int32_t first_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    thread_error = this->disable_thread_safety();
    if (thread_error != FT_ERR_SUCCESS)
        first_error = thread_error;
    if (this->_entries != ft_nullptr)
        cma_free(this->_entries);
    this->_entries = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

template<typename ElementType>
uint32_t ft_loot_table<ElementType>::move(ft_loot_table &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(
            static_cast<ft_loot_table &&>(other))));
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::disable_thread_safety()
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

template<typename ElementType>
ft_bool ft_loot_table<ElementType>::is_thread_safe() const
{
    return (this->_mutex != ft_nullptr);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::lock_table(ft_bool *lock_acquired) const noexcept
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
void ft_loot_table<ElementType>::unlock_table(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::reserve_capacity(ft_size_t requested_capacity)
{
    if (requested_capacity <= this->_capacity)
        return (FT_ERR_SUCCESS);
    if (requested_capacity > SIZE_MAX / sizeof(entry))
        return (FT_ERR_OUT_OF_RANGE);
    entry *new_entries = static_cast<entry *>(
        cma_realloc(this->_entries, requested_capacity * sizeof(entry)));
    if (new_entries == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    this->_entries = new_entries;
    this->_capacity = requested_capacity;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::select_entry_locked(ft_size_t &selected_index) const noexcept
{
    if (this->_size == 0U)
        return (FT_ERR_EMPTY);
    const uint32_t total_weight = this->_entries[this->_size - 1U].cumulative_weight;
    if (total_weight == OVERFLOW_WEIGHT)
        return (FT_ERR_OUT_OF_RANGE);
    const int32_t roll = ft_dice_roll(1, static_cast<int32_t>(total_weight));
    if (roll < 1)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t first = 0U;
    ft_size_t end = this->_size;
    while (first < end)
    {
        const ft_size_t midpoint = first + (end - first) / 2U;
        if (this->_entries[midpoint].cumulative_weight < static_cast<uint32_t>(roll))
            first = midpoint + 1U;
        else
            end = midpoint;
    }
    selected_index = first;
    return (FT_ERR_SUCCESS);
}

template<typename ElementType>
int32_t ft_loot_table<ElementType>::add_element(ElementType *element_pointer, int32_t weight, int32_t rarity)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_loot_table::add_element");
    if (weight <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (rarity < 0)
        rarity = 0;
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_table(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    int32_t operation_error = FT_ERR_SUCCESS;
    if (this->_size == this->_capacity)
    {
        constexpr ft_size_t MAX_CAPACITY = SIZE_MAX / sizeof(entry);
        ft_size_t next_capacity = 4U;
        if (this->_capacity > MAX_CAPACITY / 2U)
            next_capacity = MAX_CAPACITY;
        else if (this->_capacity != 0U)
            next_capacity = this->_capacity * 2U;
        if (next_capacity == this->_capacity)
            operation_error = FT_ERR_OUT_OF_RANGE;
        else
            operation_error = this->reserve_capacity(next_capacity);
    }
    if (operation_error == FT_ERR_SUCCESS)
    {
        uint32_t cumulative_weight = static_cast<uint32_t>(rng_loot_effective_weight(weight, rarity));
        if (this->_size != 0U)
            cumulative_weight += this->_entries[this->_size - 1U].cumulative_weight;
        if (cumulative_weight > OVERFLOW_WEIGHT)
            cumulative_weight = OVERFLOW_WEIGHT;
        this->_entries[this->_size] = {element_pointer, cumulative_weight};
        this->_size += 1U;
    }
    this->unlock_table(lock_acquired);
    return (operation_error);
}

template<typename ElementType>
Pair<int32_t, ElementType *> ft_loot_table<ElementType>::get_random_loot() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_loot_table::get_random_loot");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_table(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, ElementType *>(lock_error, ft_nullptr));
    ft_size_t selected_index = 0U;
    const int32_t operation_error = this->select_entry_locked(selected_index);
    ElementType *item = ft_nullptr;
    if (operation_error == FT_ERR_SUCCESS)
        item = this->_entries[selected_index].item;
    this->unlock_table(lock_acquired);
    return (Pair<int32_t, ElementType *>(operation_error, item));
}

template<typename ElementType>
Pair<int32_t, ElementType *> ft_loot_table<ElementType>::pop_random_loot()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_loot_table::pop_random_loot");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_table(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (Pair<int32_t, ElementType *>(lock_error, ft_nullptr));
    ft_size_t index = 0U;
    const int32_t operation_error = this->select_entry_locked(index);
    ElementType *item = ft_nullptr;
    if (operation_error == FT_ERR_SUCCESS)
    {
        item = this->_entries[index].item;
        uint32_t removed_weight = this->_entries[index].cumulative_weight;
        if (index != 0U)
            removed_weight -= this->_entries[index - 1U].cumulative_weight;
        this->_size -= 1U;
        while (index < this->_size)
        {
            this->_entries[index] = this->_entries[index + 1U];
            this->_entries[index].cumulative_weight -= removed_weight;
            index += 1U;
        }
    }
    this->unlock_table(lock_acquired);
    return (Pair<int32_t, ElementType *>(operation_error, item));
}

template<typename ElementType>
ft_size_t ft_loot_table<ElementType>::size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_loot_table::size");
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_table(&lock_acquired) != FT_ERR_SUCCESS)
        return (0U);
    const ft_size_t size = this->_size;
    this->unlock_table(lock_acquired);
    return (size);
}

#ifdef LIBFT_TEST_BUILD
template<typename ElementType>
pt_recursive_mutex *ft_loot_table<ElementType>::get_mutex_for_validation() const noexcept
{
    return (this->_mutex);
}
#endif

#endif
