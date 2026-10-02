#include "../PThread/pthread_internal.hpp"
#include "game_inventory.hpp"
#include "../Errno/errno.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include <functional>
#include <limits>

thread_local int32_t game_inventory::_last_error = FT_ERR_SUCCESS;

game_inventory::game_inventory() noexcept
    : _items(), _capacity(0), _used_slots(0), _weight_limit(0),
      _current_weight(0), _next_slot(0), _revision(0U),
      _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_inventory::~game_inventory() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_inventory::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_inventory::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    const int32_t destroy_error = this->_items.destroy();
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    const int32_t map_initialize_error = this->_items.initialize();
    if (map_initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(map_initialize_error);
        return (map_initialize_error);
    }
    this->_used_slots = 0;
    this->_current_weight = 0;
    this->_next_slot = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)this->bump_revision();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::initialize(ft_size_t capacity, int32_t weight_limit) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_capacity = capacity;
    this->_weight_limit = weight_limit;
    (void)this->bump_revision();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::initialize(const game_inventory &other) noexcept
{
    int32_t initialize_error;
    ft_size_t count;
    ft_size_t index;
    const Pair<int32_t, ft_sharedptr<game_item> > *entry;
    const Pair<int32_t, ft_sharedptr<game_item> > *entry_end;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_inventory::initialize(copy)", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_capacity = other._capacity;
    this->_used_slots = other._used_slots;
    this->_weight_limit = other._weight_limit;
    this->_current_weight = other._current_weight;
    this->_next_slot = other._next_slot;
    count = other._items.size();
    entry_end = other._items.end();
    entry = entry_end - count;
    index = 0;
    while (index < count)
    {
        this->_items.insert(entry->key, entry->value);
        entry++;
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::initialize(game_inventory &&other) noexcept
{
    return (this->move(other));
}

int32_t game_inventory::move(game_inventory &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_inventory::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize(static_cast<const game_inventory &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    (void)this->bump_revision();
    this->_items.clear();
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_inventory::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_inventory::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_inventory::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_inventory::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_map<int32_t, ft_sharedptr<game_item> > &game_inventory::get_items() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_items");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_items);
}

const ft_map<int32_t, ft_sharedptr<game_item> > &game_inventory::get_items() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_items const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_items);
}

ft_size_t game_inventory::get_capacity() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_capacity");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_capacity);
}

void game_inventory::resize(ft_size_t capacity) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::resize");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
    {
        this->set_error(lock_result);
        return ;
    }
    if (this->_capacity != capacity && this->bump_revision() == FT_ERR_SUCCESS)
        this->_capacity = capacity;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_size_t game_inventory::get_used() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_used");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_used_slots);
}

void game_inventory::set_used_slots(ft_size_t used) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::set_used_slots");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
    {
        this->set_error(lock_result);
        return ;
    }
    if (this->_used_slots != used && this->bump_revision() == FT_ERR_SUCCESS)
        this->_used_slots = used;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_bool game_inventory::is_full() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::is_full");
    ft_bool is_full_result;

#if USE_INVENTORY_SLOTS
    if (this->_capacity != 0 && this->_used_slots >= this->_capacity)
        is_full_result = FT_TRUE;
    else
#endif
#if USE_INVENTORY_WEIGHT
    if (this->_weight_limit != 0 && this->_current_weight >= this->_weight_limit)
        is_full_result = FT_TRUE;
    else
#endif
        is_full_result = FT_FALSE;
    this->set_error(FT_ERR_SUCCESS);
    return (is_full_result);
}

int32_t game_inventory::get_weight_limit() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_weight_limit");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_weight_limit);
}

void game_inventory::set_weight_limit(int32_t limit) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::set_weight_limit");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
    {
        this->set_error(lock_result);
        return ;
    }
    if (this->_weight_limit != limit && this->bump_revision() == FT_ERR_SUCCESS)
        this->_weight_limit = limit;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_inventory::get_current_weight() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::get_current_weight");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_current_weight);
}

void game_inventory::set_current_weight(int32_t weight) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::set_current_weight");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
    {
        this->set_error(lock_result);
        return ;
    }
    if (this->_current_weight != weight
        && this->bump_revision() == FT_ERR_SUCCESS)
        this->_current_weight = weight;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_bool game_inventory::check_item_valid(const ft_sharedptr<game_item> &item) const noexcept
{
    if (!item)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t game_inventory::add_item(const ft_sharedptr<game_item> &item) noexcept
{
    int32_t result;

    result = this->add_item_transactional(item);
    return (this->set_error(result));
}

int32_t game_inventory::bump_revision() noexcept
{
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (FT_ERR_OUT_OF_RANGE);
    this->_revision += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::add_item_transactional(
    const ft_sharedptr<game_item> &item) noexcept
{
    game_inventory staged_inventory;
    ft_size_t source_count;
    ft_size_t source_index;
    Pair<int32_t, ft_sharedptr<game_item> > *source_item;
    ft_bool lock_acquired;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::add_item");
    if (this->check_item_valid(item) == FT_FALSE)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    lock_acquired = FT_FALSE;
    result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (this->_revision == std::numeric_limits<uint64_t>::max())
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    result = staged_inventory.initialize(this->_capacity,
        this->_weight_limit);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(result));
    }
    staged_inventory._used_slots = this->_used_slots;
    staged_inventory._current_weight = this->_current_weight;
    staged_inventory._next_slot = this->_next_slot;
    source_count = this->_items.size();
    source_item = this->_items.end() - source_count;
    source_index = 0U;
    while (source_index < source_count)
    {
        if (source_item->value)
        {
            ft_sharedptr<game_item> copied_item(new game_item());

            if (!copied_item)
                result = FT_ERR_NO_MEMORY;
            else
                result = copied_item->initialize(*source_item->value);
            if (result == FT_ERR_SUCCESS)
            {
                staged_inventory._items.insert(source_item->key,
                    copied_item);
                result = staged_inventory._items.get_error();
            }
            if (result != FT_ERR_SUCCESS)
            {
                this->unlock_internal(lock_acquired);
                return (this->set_error(result));
            }
        }
        source_item += 1;
        source_index += 1U;
    }
    result = staged_inventory.add_item_in_place(item);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.move_from(staged_inventory._items);
    if (result == FT_ERR_SUCCESS)
    {
        this->_used_slots = staged_inventory._used_slots;
        this->_current_weight = staged_inventory._current_weight;
        this->_next_slot = staged_inventory._next_slot;
        (void)this->bump_revision();
    }
    this->unlock_internal(lock_acquired);
    return (result);
}

int32_t game_inventory::get_revision(uint64_t *revision) const noexcept
{
    ft_bool lock_acquired;
    int32_t result;

    if (revision == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_POINTER));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::get_revision");
    lock_acquired = FT_FALSE;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        *revision = this->_revision;
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

static ft_bool game_inventory_transfer_metadata_matches(
    const game_item &left, const game_item &right) noexcept
{
    return (left.get_item_id() == right.get_item_id()
        && left.get_max_stack() == right.get_max_stack()
        && left.get_rarity() == right.get_rarity()
        && left.get_width() == right.get_width()
        && left.get_height() == right.get_height()
        && left.get_modifier1_id() == right.get_modifier1_id()
        && left.get_modifier1_value() == right.get_modifier1_value()
        && left.get_modifier2_id() == right.get_modifier2_id()
        && left.get_modifier2_value() == right.get_modifier2_value()
        && left.get_modifier3_id() == right.get_modifier3_id()
        && left.get_modifier3_value() == right.get_modifier3_value()
        && left.get_modifier4_id() == right.get_modifier4_id()
        && left.get_modifier4_value() == right.get_modifier4_value()
        ? FT_TRUE : FT_FALSE);
}

int32_t game_inventory::transfer_to(game_inventory &destination,
    int32_t source_slot, int32_t quantity, uint64_t expected_source_revision,
    uint64_t expected_destination_revision) noexcept
{
    game_inventory staged_source;
    game_inventory staged_destination;
    game_inventory backup_destination;
    game_inventory *first_inventory;
    game_inventory *second_inventory;
    ft_bool first_inventory_locked;
    ft_bool second_inventory_locked;
    ft_bool first_map_locked;
    ft_bool second_map_locked;
    ft_bool first_map_lock_acquired;
    ft_bool second_map_lock_acquired;
    ft_bool destination_backup_ready;
    int32_t result;
    ft_size_t index;
    ft_size_t entry_count;
    Pair<int32_t, ft_sharedptr<game_item> > *entry;
    Pair<int32_t, ft_sharedptr<game_item> > *source_entry;
    ft_sharedptr<game_item> moved_item;
    int32_t source_stack;
    int32_t remaining;
    int32_t requested_quantity;
    int32_t area;
#if USE_INVENTORY_WEIGHT
    int32_t transfer_weight;
#endif

    if (&destination == this)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (quantity <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    requested_quantity = quantity;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::transfer_to source");
    errno_abort_if_uninitialised_or_destroyed(destination._initialised_state,
        "game_inventory::transfer_to destination");
    first_inventory = this;
    second_inventory = &destination;
    if (std::less<const game_inventory *>()(second_inventory, first_inventory))
    {
        first_inventory = &destination;
        second_inventory = this;
    }
    first_inventory_locked = FT_FALSE;
    second_inventory_locked = FT_FALSE;
    first_map_locked = FT_FALSE;
    second_map_locked = FT_FALSE;
    first_map_lock_acquired = FT_FALSE;
    second_map_lock_acquired = FT_FALSE;
    destination_backup_ready = FT_FALSE;
    result = first_inventory->lock_internal(&first_inventory_locked);
    if (result == FT_ERR_SUCCESS)
        result = second_inventory->lock_internal(&second_inventory_locked);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    if (this->_revision != expected_source_revision
        || destination._revision != expected_destination_revision)
    {
        result = FT_ERR_INVALID_STATE;
        goto transfer_cleanup;
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max()
        || destination._revision == std::numeric_limits<uint64_t>::max())
    {
        result = FT_ERR_OUT_OF_RANGE;
        goto transfer_cleanup;
    }
    source_entry = this->_items.find(source_slot);
    if (source_entry == this->_items.end() || !source_entry->value)
    {
        result = FT_ERR_NOT_FOUND;
        goto transfer_cleanup;
    }
    source_stack = source_entry->value->get_stack_size();
    if (source_stack <= 0 || quantity > source_stack
        || source_entry->value->get_max_stack() <= 0
        || source_entry->value->get_width() < 0
        || source_entry->value->get_height() < 0
        || (source_entry->value->get_width() != 0
            && source_entry->value->get_height() > FT_INT32_MAX
                / source_entry->value->get_width()))
    {
        result = FT_ERR_INVALID_ARGUMENT;
        goto transfer_cleanup;
    }
    area = source_entry->value->get_width()
        * source_entry->value->get_height();
    if (area != 0 && quantity > FT_INT32_MAX / area)
    {
        result = FT_ERR_OUT_OF_RANGE;
        goto transfer_cleanup;
    }
#if USE_INVENTORY_WEIGHT
    transfer_weight = quantity * area;
    if (destination._weight_limit != 0
        && (destination._current_weight < 0
            || transfer_weight > destination._weight_limit
            || destination._current_weight
                > destination._weight_limit - transfer_weight))
    {
        result = FT_ERR_FULL;
        goto transfer_cleanup;
    }
#endif
    if (destination._current_weight > FT_INT32_MAX - quantity
        || this->_current_weight < quantity
#if USE_INVENTORY_SLOTS
        || (quantity == source_stack
            && this->_used_slots < static_cast<ft_size_t>(area)))
#else
        )
#endif
    {
        result = FT_ERR_OUT_OF_RANGE;
        goto transfer_cleanup;
    }
    result = staged_source.initialize(this->_capacity, this->_weight_limit);
    if (result == FT_ERR_SUCCESS)
        result = staged_destination.initialize(destination._capacity,
            destination._weight_limit);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    staged_source._used_slots = this->_used_slots;
    staged_source._current_weight = this->_current_weight;
    staged_source._next_slot = this->_next_slot;
    staged_destination._used_slots = destination._used_slots;
    staged_destination._current_weight = destination._current_weight;
    staged_destination._next_slot = destination._next_slot;
    result = backup_destination.initialize();
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    index = 0U;
    entry_count = this->_items.size();
    entry = entry_count == 0U ? ft_nullptr : this->_items.end() - entry_count;
    while (index < entry_count)
    {
        if (entry->value)
        {
            ft_sharedptr<game_item> copy(new (std::nothrow) game_item());

            if (!copy)
            {
                result = FT_ERR_NO_MEMORY;
                goto transfer_cleanup;
            }
            result = copy->initialize(*entry->value);
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
            staged_source._items.insert(entry->key, copy);
            result = staged_source._items.get_error();
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
        }
        else
        {
            staged_source._items.insert(entry->key, entry->value);
            result = staged_source._items.get_error();
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
        }
        entry += 1;
        index += 1U;
    }
    index = 0U;
    entry_count = destination._items.size();
    entry = entry_count == 0U ? ft_nullptr
        : destination._items.end() - entry_count;
    while (index < entry_count)
    {
        if (entry->value)
        {
            ft_sharedptr<game_item> copy(new (std::nothrow) game_item());

            if (!copy)
            {
                result = FT_ERR_NO_MEMORY;
                goto transfer_cleanup;
            }
            result = copy->initialize(*entry->value);
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
            staged_destination._items.insert(entry->key, copy);
            result = staged_destination._items.get_error();
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
        }
        else
        {
            staged_destination._items.insert(entry->key, entry->value);
            result = staged_destination._items.get_error();
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
        }
        entry += 1;
        index += 1U;
    }
    source_entry = staged_source._items.find(source_slot);
    if (source_entry == staged_source._items.end() || !source_entry->value)
    {
        result = FT_ERR_INTERNAL;
        goto transfer_cleanup;
    }
    moved_item.initialize(new (std::nothrow) game_item());
    if (!moved_item)
    {
        result = FT_ERR_NO_MEMORY;
        goto transfer_cleanup;
    }
    result = moved_item->initialize(*source_entry->value);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    moved_item->set_stack_size(quantity);

    /* Debit/remove only in the staged source copy. */
    if (quantity == source_stack)
    {
        staged_source._items.remove(source_slot);
        result = staged_source._items.get_error();
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
#if USE_INVENTORY_SLOTS
        staged_source._used_slots -= static_cast<ft_size_t>(area);
#endif
    }
    else
        source_entry->value->set_stack_size(source_stack - quantity);
    staged_source._current_weight -= quantity;

    /* Merge only completely compatible item metadata. */
    remaining = requested_quantity;
    entry_count = staged_destination._items.size();
    entry = entry_count == 0U ? ft_nullptr
        : staged_destination._items.end() - entry_count;
    index = 0U;
    while (index < entry_count && remaining > 0)
    {
        if (entry->value
            && game_inventory_transfer_metadata_matches(*entry->value,
                *moved_item) == FT_TRUE)
        {
            const int32_t room = entry->value->get_max_stack()
                - entry->value->get_stack_size();
            if (room > 0)
            {
                const int32_t amount = remaining < room ? remaining : room;
                entry->value->add_to_stack(amount);
                remaining -= amount;
            }
        }
        entry += 1;
        index += 1U;
    }
    while (remaining > 0)
    {
        const int32_t max_stack = moved_item->get_max_stack();
        const int32_t amount = remaining < max_stack ? remaining : max_stack;
        const int32_t slot_usage = moved_item->get_width()
            * moved_item->get_height();
        ft_sharedptr<game_item> stack(new (std::nothrow) game_item());

#if USE_INVENTORY_SLOTS
        if (destination._capacity != 0
            && (staged_destination._used_slots > destination._capacity
                || static_cast<ft_size_t>(slot_usage)
                    > destination._capacity - staged_destination._used_slots))
        {
            result = FT_ERR_FULL;
            goto transfer_cleanup;
        }
        if (static_cast<ft_size_t>(slot_usage)
            > std::numeric_limits<ft_size_t>::max()
                - staged_destination._used_slots)
        {
            result = FT_ERR_OUT_OF_RANGE;
            goto transfer_cleanup;
        }
#endif
        if (!stack)
        {
            result = FT_ERR_NO_MEMORY;
            goto transfer_cleanup;
        }
        result = stack->initialize(*moved_item);
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        stack->set_stack_size(amount);
        if (staged_destination._next_slot == FT_INT32_MAX)
        {
            result = FT_ERR_OUT_OF_RANGE;
            goto transfer_cleanup;
        }
        staged_destination._items.insert(staged_destination._next_slot, stack);
        result = staged_destination._items.get_error();
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        staged_destination._next_slot += 1;
#if USE_INVENTORY_SLOTS
        staged_destination._used_slots += static_cast<ft_size_t>(slot_usage);
#endif
        remaining -= amount;
    }
    staged_destination._current_weight += requested_quantity;

    /* Both live maps are locked before commit; move_from only transfers
     * already-allocated map storage, so no allocation/capacity failure can
     * occur after the first live map is replaced. */
    result = first_inventory->_items.lock(&first_map_lock_acquired);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    first_map_locked = FT_TRUE;
    result = second_inventory->_items.lock(&second_map_lock_acquired);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    second_map_locked = FT_TRUE;
    /* Keep the old destination map as a rollback image until both commits
     * succeed. The maps are initialized and locked, so these storage moves
     * allocate nothing; this also protects the source if destination install
     * reports an error. */
    result = backup_destination._items.move_from(destination._items);
    if (result == FT_ERR_SUCCESS)
        destination_backup_ready = FT_TRUE;
    if (result == FT_ERR_SUCCESS)
        result = destination._items.move_from(staged_destination._items);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.move_from(staged_source._items);
    if (result != FT_ERR_SUCCESS && destination_backup_ready == FT_TRUE)
    {
        const int32_t rollback_result = destination._items.move_from(
            backup_destination._items);

        if (rollback_result != FT_ERR_SUCCESS)
            result = FT_ERR_INTERNAL;
    }
    if (result == FT_ERR_SUCCESS)
    {
        this->_used_slots = staged_source._used_slots;
        this->_current_weight = staged_source._current_weight;
        this->_next_slot = staged_source._next_slot;
        destination._used_slots = staged_destination._used_slots;
        destination._current_weight = staged_destination._current_weight;
        destination._next_slot = staged_destination._next_slot;
        (void)this->bump_revision();
        (void)destination.bump_revision();
    }

transfer_cleanup:
    if (second_map_locked == FT_TRUE)
        (void)second_inventory->_items.unlock(second_map_lock_acquired);
    if (first_map_locked == FT_TRUE)
        (void)first_inventory->_items.unlock(first_map_lock_acquired);
    if (second_inventory_locked == FT_TRUE)
        (void)second_inventory->unlock_internal(FT_TRUE);
    if (first_inventory_locked == FT_TRUE)
        (void)first_inventory->unlock_internal(FT_TRUE);
    return (this->set_error(result));
}

int32_t game_inventory::add_item_in_place(
    const ft_sharedptr<game_item> &item) noexcept
{
    int32_t remaining;
    int32_t item_id;
    Pair<int32_t, ft_sharedptr<game_item> > *item_pointer;
    Pair<int32_t, ft_sharedptr<game_item> > *item_end;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::add_item_in_place");
    if (this->check_item_valid(item) == FT_FALSE)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    remaining = item->get_stack_size();
#if USE_INVENTORY_WEIGHT
    int32_t projected_weight;

    if (item->get_stack_size() < 0 || item->get_width() < 0
        || item->get_height() < 0
        || (item->get_width() != 0
            && item->get_height() > FT_INT32_MAX / item->get_width()))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (item->get_width() * item->get_height() != 0
        && item->get_stack_size() > FT_INT32_MAX
            / (item->get_width() * item->get_height()))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    projected_weight = item->get_stack_size() * item->get_width()
        * item->get_height();
    if (this->_weight_limit != 0
        && (projected_weight > this->_weight_limit
            || this->_current_weight > this->_weight_limit - projected_weight))
    {
        this->set_error(FT_ERR_FULL);
        return (FT_ERR_FULL);
    }
#endif
    item_id = item->get_item_id();
    item_pointer = this->_items.end() - this->_items.size();
    item_end = this->_items.end();
    while (item_pointer != item_end && remaining > 0)
    {
        if (item_pointer->value)
        {
            if (item_pointer->value->get_item_id() == item_id)
            {
                int32_t free_space;
                int32_t amount_to_add;

                free_space = item_pointer->value->get_max_stack() - item_pointer->value->get_stack_size();
                if (free_space > 0)
                {
                    if (remaining < free_space)
                        amount_to_add = remaining;
                    else
                        amount_to_add = free_space;
                    item_pointer->value->add_to_stack(amount_to_add);
                    this->_current_weight += amount_to_add;
                    remaining -= amount_to_add;
                }
            }
        }
        item_pointer++;
    }
    while (remaining > 0)
    {
#if USE_INVENTORY_SLOTS
        int32_t slot_usage;

        slot_usage = item->get_width() * item->get_height();
        if (this->_capacity != 0 && this->_used_slots + slot_usage > this->_capacity)
        {
            this->set_error(FT_ERR_FULL);
            return (FT_ERR_FULL);
        }
#endif
        ft_sharedptr<game_item> new_item(new game_item());
        int32_t amount_to_add;

        if (!new_item)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            return (FT_ERR_NO_MEMORY);
        }
        const int32_t initialize_error = new_item->initialize(*item);
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->set_error(initialize_error);
            return (initialize_error);
        }
        if (remaining < new_item->get_max_stack())
            amount_to_add = remaining;
        else
            amount_to_add = new_item->get_max_stack();
        new_item->set_stack_size(amount_to_add);
        this->_items.insert(this->_next_slot, new_item);
        if (this->_items.get_error() != FT_ERR_SUCCESS)
            return (this->set_error(this->_items.get_error()));
        this->_next_slot++;
        this->_current_weight += amount_to_add;
#if USE_INVENTORY_SLOTS
        this->_used_slots += slot_usage;
#endif
        remaining -= amount_to_add;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void game_inventory::remove_item(int32_t slot) noexcept
{
    Pair<int32_t, ft_sharedptr<game_item> > *entry;
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::remove_item");
    lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    entry = this->_items.find(slot);
    if (entry == this->_items.end())
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return ;
    }
    if (entry->value)
    {
        this->_current_weight -= entry->value->get_stack_size();
#if USE_INVENTORY_SLOTS
        const int32_t area = entry->value->get_width()
            * entry->value->get_height();
        if (area >= 0 && this->_used_slots >= static_cast<ft_size_t>(area))
            this->_used_slots -= static_cast<ft_size_t>(area);
#endif
    }
    this->_items.remove(slot);
    (void)this->bump_revision();
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_inventory::count_item(int32_t item_id) const noexcept
{
    int32_t total;
    ft_size_t index;
    const Pair<int32_t, ft_sharedptr<game_item> > *item_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::count_item");
    total = 0;
    index = 0;
    while (index < static_cast<ft_size_t>(this->_next_slot))
    {
        item_pointer = this->_items.find(static_cast<int32_t>(index));
        if (item_pointer != this->_items.end() && item_pointer->value)
        {
            if (item_pointer->value->get_item_id() == item_id)
                total += item_pointer->value->get_stack_size();
        }
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (total);
}

ft_bool game_inventory::has_item(int32_t item_id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::has_item");
    const ft_bool result = this->count_item(item_id) > 0;
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t game_inventory::count_rarity(int32_t rarity) const noexcept
{
    int32_t total;
    ft_size_t index;
    const Pair<int32_t, ft_sharedptr<game_item> > *item_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::count_rarity");
    total = 0;
    index = 0;
    while (index < static_cast<ft_size_t>(this->_next_slot))
    {
        item_pointer = this->_items.find(static_cast<int32_t>(index));
        if (item_pointer != this->_items.end() && item_pointer->value)
        {
            if (item_pointer->value->get_rarity() == rarity)
                total += item_pointer->value->get_stack_size();
        }
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (total);
}

ft_bool game_inventory::has_rarity(int32_t rarity) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::has_rarity");
    const ft_bool result = this->count_rarity(rarity) > 0;
    this->set_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t game_inventory::set_error(int32_t error_code) noexcept
{
    game_inventory::_last_error = error_code;
    return (error_code);
}

int32_t game_inventory::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_inventory::get_error");
    return (game_inventory::_last_error);
}

const char *game_inventory::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_inventory::get_error_str");
    return (ft_strerror(this->get_error()));
}
