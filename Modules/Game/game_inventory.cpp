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

static ft_bool game_inventory_transfer_metadata_matches(
	const game_item &left, const game_item &right) noexcept;

game_inventory::game_inventory() noexcept
    : _items(), _transfer_cache(), _quantity_remove_cache(), _capacity(0), _used_slots(0), _weight_limit(0),
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
    this->_transfer_cache.clear();
    this->_quantity_remove_cache.clear();
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
    this->_transfer_cache.clear();
    this->_quantity_remove_cache.clear();
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

#ifdef LIBFT_TEST_BUILD
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

#endif

int32_t game_inventory::get_snapshot(
    std::vector<game_inventory_item_snapshot> &items_out,
    uint64_t *revision_out) const noexcept
{
    game_inventory_snapshot snapshot;
    int32_t result;

    if (revision_out == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = this->get_snapshot(snapshot);
    if (result == FT_ERR_SUCCESS)
    {
        items_out.swap(snapshot.items);
        *revision_out = snapshot.revision;
    }
    return (this->set_error(result));
}

int32_t game_inventory::get_snapshot(
    game_inventory_snapshot &snapshot_out) const noexcept
{
    game_inventory_snapshot staged_snapshot;
    const Pair<int32_t, ft_sharedptr<game_item> > *entry;
    const Pair<int32_t, ft_sharedptr<game_item> > *end_entry;
    ft_bool inventory_lock_acquired;
    ft_bool map_lock_acquired;
    ft_size_t entry_count;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::get_snapshot");
    inventory_lock_acquired = FT_FALSE;
    map_lock_acquired = FT_FALSE;
    result = this->lock_internal(&inventory_lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.lock(&map_lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        staged_snapshot.capacity = this->_capacity;
        staged_snapshot.used_slots = this->_used_slots;
        staged_snapshot.weight_limit = this->_weight_limit;
        staged_snapshot.current_weight = this->_current_weight;
        staged_snapshot.next_slot = this->_next_slot;
        staged_snapshot.revision = this->_revision;
        entry_count = this->_items.size();
        try
        {
            staged_snapshot.items.reserve(entry_count);
        }
        catch (const std::bad_alloc &)
        {
            result = FT_ERR_NO_MEMORY;
        }
        if (result == FT_ERR_SUCCESS)
        {
            end_entry = this->_items.end();
            entry = end_entry - entry_count;
            while (entry != end_entry)
            {
                if (!entry->value)
                {
                    result = FT_ERR_INVALID_STATE;
                    break ;
                }
                game_inventory_item_snapshot snapshot = {};
                snapshot.slot = entry->key;
                snapshot.item_id = entry->value->get_item_id();
                snapshot.instance_id = entry->value->get_instance_id();
                snapshot.stack_size = entry->value->get_stack_size();
                snapshot.max_stack = entry->value->get_max_stack();
                snapshot.rarity = entry->value->get_rarity();
                snapshot.width = entry->value->get_width();
                snapshot.height = entry->value->get_height();
                snapshot.modifier1_id = entry->value->get_modifier1_id();
                snapshot.modifier1_value = entry->value->get_modifier1_value();
                snapshot.modifier2_id = entry->value->get_modifier2_id();
                snapshot.modifier2_value = entry->value->get_modifier2_value();
                snapshot.modifier3_id = entry->value->get_modifier3_id();
                snapshot.modifier3_value = entry->value->get_modifier3_value();
                snapshot.modifier4_id = entry->value->get_modifier4_id();
                snapshot.modifier4_value = entry->value->get_modifier4_value();
                if (snapshot.item_id <= 0 || snapshot.stack_size <= 0
                    || snapshot.max_stack <= 0
                    || snapshot.stack_size > snapshot.max_stack
                    || snapshot.width <= 0 || snapshot.height <= 0)
                {
                    result = FT_ERR_INVALID_STATE;
                    break ;
                }
                try
                {
                    staged_snapshot.items.push_back(snapshot);
                }
                catch (const std::bad_alloc &)
                {
                    result = FT_ERR_NO_MEMORY;
                    break ;
                }
                ++entry;
            }
        }
    }
    if (map_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->_items.unlock(map_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    if (inventory_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->unlock_internal(
            inventory_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    if (result == FT_ERR_SUCCESS)
    {
        snapshot_out.capacity = staged_snapshot.capacity;
        snapshot_out.used_slots = staged_snapshot.used_slots;
        snapshot_out.weight_limit = staged_snapshot.weight_limit;
        snapshot_out.current_weight = staged_snapshot.current_weight;
        snapshot_out.next_slot = staged_snapshot.next_slot;
        snapshot_out.revision = staged_snapshot.revision;
        snapshot_out.items.swap(staged_snapshot.items);
    }
    return (this->set_error(result));
}

int32_t game_inventory::copy_item_at_revision(int32_t slot,
    uint64_t expected_revision,
    ft_sharedptr<game_item> **item_out) const noexcept
{
    const Pair<int32_t, ft_sharedptr<game_item> > *entry;
    ft_bool inventory_lock_acquired = FT_FALSE;
    ft_bool map_lock_acquired = FT_FALSE;
    int32_t result = FT_ERR_SUCCESS;
    ft_sharedptr<game_item> *staged_item = ft_nullptr;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::copy_item_at_revision");
    if (item_out == ft_nullptr || slot < 0 || expected_revision == 0U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    *item_out = ft_nullptr;
    result = this->lock_internal(&inventory_lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.lock(&map_lock_acquired);
    if (result == FT_ERR_SUCCESS && this->_revision != expected_revision)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
    {
        entry = this->_items.find(slot);
        if (entry == this->_items.end() || !entry->value)
            result = FT_ERR_NOT_FOUND;
    }
    if (result == FT_ERR_SUCCESS)
    {
        staged_item = new (std::nothrow) ft_sharedptr<game_item>();
        if (staged_item == ft_nullptr)
            result = FT_ERR_NO_MEMORY;
    }
    if (result == FT_ERR_SUCCESS)
        result = staged_item->initialize(entry->value);
    if (map_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->_items.unlock(map_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    if (inventory_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->unlock_internal(
            inventory_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    if (result != FT_ERR_SUCCESS)
    {
        if (staged_item != ft_nullptr)
        {
            (void)staged_item->destroy();
            delete staged_item;
        }
        return (this->set_error(result));
    }
    *item_out = staged_item;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_inventory::restore_snapshot(
    const game_inventory_snapshot &snapshot) noexcept
{
    game_inventory staged_inventory;
    game_inventory backup_inventory;
    int64_t calculated_weight = 0;
    uint64_t calculated_slots = 0U;
    int32_t maximum_slot = -1;
    int32_t result;
    ft_bool backup_ready = FT_FALSE;
    ft_bool inventory_lock_acquired = FT_FALSE;
    ft_bool map_lock_acquired = FT_FALSE;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || snapshot.next_slot < 0 || snapshot.weight_limit < 0
        || snapshot.current_weight < 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = staged_inventory.initialize(snapshot.capacity,
        snapshot.weight_limit);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    for (const game_inventory_item_snapshot &entry : snapshot.items)
    {
        if (entry.slot < 0 || entry.slot >= snapshot.next_slot
            || entry.item_id <= 0 || entry.stack_size <= 0
            || entry.max_stack <= 0 || entry.stack_size > entry.max_stack
            || entry.width <= 0 || entry.height <= 0
            || (entry.instance_id != 0U
                && (entry.stack_size != 1 || entry.max_stack != 1)))
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        for (const game_inventory_item_snapshot &prior : snapshot.items)
        {
            if (&prior == &entry)
                break ;
            if (prior.slot == entry.slot
                || (entry.instance_id != 0U
                    && prior.instance_id == entry.instance_id))
                return (this->set_error(FT_ERR_ALREADY_EXISTS));
        }
        ft_sharedptr<game_item> item(new (std::nothrow) game_item());
        if (!item)
            return (this->set_error(FT_ERR_NO_MEMORY));
        result = item->initialize();
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        item->set_item_id(entry.item_id);
        item->set_instance_id(entry.instance_id);
        item->set_stack_size(entry.stack_size);
        item->set_max_stack(entry.max_stack);
        item->set_rarity(entry.rarity);
        item->set_width(entry.width);
        item->set_height(entry.height);
        item->set_modifier1_id(entry.modifier1_id);
        item->set_modifier1_value(entry.modifier1_value);
        item->set_modifier2_id(entry.modifier2_id);
        item->set_modifier2_value(entry.modifier2_value);
        item->set_modifier3_id(entry.modifier3_id);
        item->set_modifier3_value(entry.modifier3_value);
        item->set_modifier4_id(entry.modifier4_id);
        item->set_modifier4_value(entry.modifier4_value);
        if (this->check_item_valid(item) != FT_TRUE)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        if (staged_inventory._items.find(entry.slot)
            != staged_inventory._items.end())
            return (this->set_error(FT_ERR_ALREADY_EXISTS));
        staged_inventory._items.insert(entry.slot, item);
        result = staged_inventory._items.get_error();
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        calculated_weight += static_cast<int64_t>(entry.stack_size);
        calculated_slots += static_cast<uint64_t>(entry.width)
            * static_cast<uint64_t>(entry.height);
        if (entry.slot > maximum_slot)
            maximum_slot = entry.slot;
    }
    if (calculated_weight != snapshot.current_weight
        || snapshot.next_slot <= maximum_slot
        || calculated_weight > std::numeric_limits<int32_t>::max()
        || calculated_slots > std::numeric_limits<ft_size_t>::max()
#if USE_INVENTORY_SLOTS
        || calculated_slots != static_cast<uint64_t>(snapshot.used_slots)
        || (snapshot.capacity != 0U
            && calculated_slots > static_cast<uint64_t>(snapshot.capacity))
#endif
        || (snapshot.weight_limit != 0
            && calculated_weight > snapshot.weight_limit))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    staged_inventory._used_slots = snapshot.used_slots;
    staged_inventory._current_weight = snapshot.current_weight;
    staged_inventory._next_slot = snapshot.next_slot;
    staged_inventory._revision = snapshot.revision;
    result = backup_inventory.initialize(this->_capacity,
        this->_weight_limit);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = this->lock_internal(&inventory_lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.lock(&map_lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = backup_inventory._items.move_from(this->_items);
        if (result == FT_ERR_SUCCESS)
            backup_ready = FT_TRUE;
    }
    if (result == FT_ERR_SUCCESS)
        result = this->_items.move_from(staged_inventory._items);
    if (result != FT_ERR_SUCCESS && backup_ready == FT_TRUE)
    {
        const int32_t rollback_result = this->_items.move_from(
            backup_inventory._items);
        if (rollback_result != FT_ERR_SUCCESS)
            result = FT_ERR_INTERNAL;
    }
    if (result == FT_ERR_SUCCESS)
    {
        this->_capacity = snapshot.capacity;
        this->_used_slots = snapshot.used_slots;
        this->_weight_limit = snapshot.weight_limit;
        this->_current_weight = snapshot.current_weight;
        this->_next_slot = snapshot.next_slot;
        this->_revision = snapshot.revision;
    }
    if (map_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->_items.unlock(
            map_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    if (inventory_lock_acquired == FT_TRUE)
    {
        const int32_t unlock_result = this->unlock_internal(
            inventory_lock_acquired);
        if (result == FT_ERR_SUCCESS && unlock_result != FT_ERR_SUCCESS)
            result = unlock_result;
    }
    return (this->set_error(result));
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
    if (item->get_instance_id() != 0U
        && (item->get_max_stack() != 1 || item->get_stack_size() != 1))
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t game_inventory::add_item(const ft_sharedptr<game_item> &item) noexcept
{
    int32_t result;

    result = this->add_item_transactional(item, 0U, FT_FALSE, ft_nullptr);
    return (this->set_error(result));
}

int32_t game_inventory::add_item_revisioned(
    const ft_sharedptr<game_item> &item, uint64_t expected_revision,
    int32_t *inserted_slot_out) noexcept
{
    int32_t result;

    if (inserted_slot_out == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = this->add_item_transactional(item, expected_revision, FT_TRUE,
        inserted_slot_out);
    return (this->set_error(result));
}

int32_t game_inventory::clear_items() noexcept
{
	ft_bool lock_acquired;
	int32_t result;

	errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
		"game_inventory::clear_items");
	lock_acquired = FT_FALSE;
	result = this->lock_internal(&lock_acquired);
	if (result != FT_ERR_SUCCESS)
		return (this->set_error(result));
	result = this->bump_revision();
	if (result == FT_ERR_SUCCESS)
	{
		this->_items.clear();
		this->_used_slots = 0U;
		this->_current_weight = 0;
		this->_next_slot = 0;
	}
	(void)this->unlock_internal(lock_acquired);
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
    const ft_sharedptr<game_item> &item, uint64_t expected_revision,
    ft_bool check_revision, int32_t *inserted_slot_out) noexcept
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
    if (inserted_slot_out != ft_nullptr
        && (item->get_instance_id() == 0U
            || item->get_stack_size() != 1))
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
    if (check_revision != FT_FALSE
        && this->_revision != expected_revision)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_INVALID_STATE));
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
    int32_t inserted_slot = -1;
    if (result == FT_ERR_SUCCESS && inserted_slot_out != ft_nullptr)
    {
        Pair<int32_t, ft_sharedptr<game_item> > *inserted_item;

        if (staged_inventory._next_slot <= 0)
            result = FT_ERR_INVALID_STATE;
        else
        {
            inserted_slot = staged_inventory._next_slot - 1;
            inserted_item = staged_inventory._items.find(inserted_slot);
            if (inserted_item == staged_inventory._items.end()
                || !inserted_item->value
                || inserted_item->value->get_instance_id()
                    != item->get_instance_id())
                result = FT_ERR_INVALID_STATE;
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = this->_items.move_from(staged_inventory._items);
    if (result == FT_ERR_SUCCESS)
    {
        this->_used_slots = staged_inventory._used_slots;
        this->_current_weight = staged_inventory._current_weight;
        this->_next_slot = staged_inventory._next_slot;
        result = this->bump_revision();
        if (result == FT_ERR_SUCCESS && inserted_slot_out != ft_nullptr)
            *inserted_slot_out = inserted_slot;
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

int32_t game_inventory::restore_revision(uint64_t revision) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_revision = revision;
    this->_transfer_cache.clear();
    this->_quantity_remove_cache.clear();
    error_code = this->unlock_internal(lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->set_error(FT_ERR_SUCCESS));
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
        && left.get_instance_id() == right.get_instance_id()
        ? FT_TRUE : FT_FALSE);
}

int32_t game_inventory::transfer_to(game_inventory &destination,
    int32_t source_slot, int32_t quantity, uint64_t expected_source_revision,
    uint64_t expected_destination_revision) noexcept
{
    return (this->transfer_to(destination, source_slot, quantity,
        expected_source_revision, expected_destination_revision, 0U, 0U,
        ft_nullptr));
}

int32_t game_inventory::transfer_to(game_inventory &destination,
    int32_t source_slot, int32_t quantity, uint64_t expected_source_revision,
    uint64_t expected_destination_revision, uint64_t transaction_id,
    game_inventory_transfer_result *result_out) noexcept
{
    return (this->transfer_to(destination, source_slot, quantity,
        expected_source_revision, expected_destination_revision, 0U,
        transaction_id, result_out));
}

int32_t game_inventory::transfer_to(game_inventory &destination,
    int32_t source_slot, int32_t quantity, uint64_t expected_source_revision,
    uint64_t expected_destination_revision, uint64_t transaction_scope_id,
    uint64_t transaction_id,
    game_inventory_transfer_result *result_out) noexcept
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
    TransferCacheEntry *cache_entry;
    std::map<std::pair<uint64_t, uint64_t>, TransferCacheEntry>::iterator cached_entry;
    std::pair<uint64_t, uint64_t> cache_key;
    ft_bool cache_replay;
#if USE_INVENTORY_WEIGHT
    int32_t transfer_weight;
#endif

    if (result_out != ft_nullptr)
    {
        result_out->transaction_scope_id = transaction_scope_id;
        result_out->transaction_id = transaction_id;
        result_out->source_revision = 0U;
        result_out->destination_revision = 0U;
        result_out->transferred_quantity = 0;
        result_out->result_code = FT_ERR_INVALID_ARGUMENT;
    }
    if ((transaction_id == 0U) != (result_out == ft_nullptr)
        || (transaction_id == 0U && transaction_scope_id != 0U))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (&destination == this)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (quantity <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    requested_quantity = quantity;
    cache_entry = ft_nullptr;
    cache_replay = FT_FALSE;
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
    if (transaction_id != 0U)
    {
        cache_key = std::make_pair(transaction_scope_id, transaction_id);
        if (this->_quantity_remove_cache.find(cache_key)
            != this->_quantity_remove_cache.end())
        {
            result = FT_ERR_INVALID_ARGUMENT;
            goto transfer_cleanup;
        }
        cached_entry = this->_transfer_cache.find(cache_key);
        if (cached_entry != this->_transfer_cache.end())
        {
            if (cached_entry->second.destination != &destination
                || cached_entry->second.source_slot != source_slot
                || cached_entry->second.quantity != quantity
                || cached_entry->second.expected_source_revision
                    != expected_source_revision
                || cached_entry->second.expected_destination_revision
                    != expected_destination_revision)
            {
                result = FT_ERR_INVALID_ARGUMENT;
                goto transfer_cleanup;
            }
            if (cached_entry->second.completed != FT_TRUE)
            {
                result = FT_ERR_INVALID_STATE;
                goto transfer_cleanup;
            }
            *result_out = cached_entry->second.result;
            result = cached_entry->second.result.result_code;
            cache_replay = FT_TRUE;
            goto transfer_cleanup;
        }
        if (this->_transfer_cache.size() >= TRANSFER_CACHE_LIMIT)
        {
            result = FT_ERR_FULL;
            goto transfer_cleanup;
        }
        try
        {
            TransferCacheEntry record;
            record.destination = &destination;
            record.source_slot = source_slot;
            record.quantity = quantity;
            record.expected_source_revision = expected_source_revision;
            record.expected_destination_revision = expected_destination_revision;
            record.transaction_scope_id = transaction_scope_id;
            record.result.transaction_scope_id = transaction_scope_id;
            record.result.transaction_id = transaction_id;
            record.result.source_revision = this->_revision;
            record.result.destination_revision = destination._revision;
            record.result.transferred_quantity = 0;
            record.result.result_code = FT_ERR_INVALID_STATE;
            record.completed = FT_FALSE;
            cache_entry = &this->_transfer_cache.insert(
                std::make_pair(cache_key, record)).first->second;
        }
        catch (...)
        {
            result = FT_ERR_NO_MEMORY;
            goto transfer_cleanup;
        }
    }
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
        || (source_entry->value->get_instance_id() != 0U
            && (source_entry->value->get_max_stack() != 1
                || source_stack != 1 || quantity != 1))
        || source_entry->value->get_width() < 0
        || source_entry->value->get_height() < 0
        || (source_entry->value->get_width() != 0
            && source_entry->value->get_height() > FT_INT32_MAX
                / source_entry->value->get_width()))
    {
        result = FT_ERR_INVALID_ARGUMENT;
        goto transfer_cleanup;
    }
    if (source_entry->value->get_instance_id() != 0U)
    {
        const ft_size_t destination_count = destination._items.size();
        Pair<int32_t, ft_sharedptr<game_item>> *destination_entry =
            destination_count == 0U ? ft_nullptr
                : destination._items.end() - destination_count;
        Pair<int32_t, ft_sharedptr<game_item>> *destination_end =
            destination_count == 0U ? ft_nullptr : destination._items.end();

        while (destination_entry != destination_end)
        {
            if (destination_entry->value
                && destination_entry->value->get_instance_id()
                    == source_entry->value->get_instance_id())
            {
                result = FT_ERR_ALREADY_EXISTS;
                goto transfer_cleanup;
            }
            ++destination_entry;
        }
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
    if (cache_entry != ft_nullptr && cache_replay != FT_TRUE)
    {
        cache_entry->result.transaction_scope_id = transaction_scope_id;
        cache_entry->result.transaction_id = transaction_id;
        cache_entry->result.source_revision = this->_revision;
        cache_entry->result.destination_revision = destination._revision;
        cache_entry->result.transferred_quantity = result == FT_ERR_SUCCESS
            ? requested_quantity : 0;
        cache_entry->result.result_code = result;
        cache_entry->completed = FT_TRUE;
        *result_out = cache_entry->result;
    }
    else if (result_out != ft_nullptr && cache_replay != FT_TRUE)
    {
        result_out->transaction_scope_id = transaction_scope_id;
        result_out->transaction_id = transaction_id;
        result_out->source_revision = first_inventory_locked == FT_TRUE
            && second_inventory_locked == FT_TRUE ? this->_revision : 0U;
        result_out->destination_revision = first_inventory_locked == FT_TRUE
            && second_inventory_locked == FT_TRUE
                ? destination._revision : 0U;
        result_out->transferred_quantity = 0;
        result_out->result_code = result;
    }
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
    uint64_t instance_id;
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
    instance_id = item->get_instance_id();
    item_pointer = this->_items.end() - this->_items.size();
    item_end = this->_items.end();
    if (instance_id != 0U)
    {
        Pair<int32_t, ft_sharedptr<game_item>> *existing = item_pointer;

        while (existing != item_end)
        {
            if (existing->value
                && existing->value->get_instance_id() == instance_id)
                return (this->set_error(FT_ERR_ALREADY_EXISTS));
            ++existing;
        }
        item_pointer = this->_items.end() - this->_items.size();
    }
    while (item_pointer != item_end && remaining > 0)
    {
        if (item_pointer->value)
        {
            if (item_pointer->value->get_item_id() == item_id
                && game_inventory_transfer_metadata_matches(
                    *item_pointer->value, *item) == FT_TRUE)
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

int32_t game_inventory::remove_item_instance(int32_t slot,
    uint64_t expected_revision, uint64_t instance_id) noexcept
{
    Pair<int32_t, ft_sharedptr<game_item> > *entry;
    ft_bool lock_acquired;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::remove_item_instance");
    if (slot < 0 || instance_id == 0U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_revision != expected_revision)
        error_code = FT_ERR_INVALID_STATE;
    else
    {
        entry = this->_items.find(slot);
        if (entry == this->_items.end() || !entry->value)
            error_code = FT_ERR_NOT_FOUND;
        else if (entry->value->get_instance_id() != instance_id
            || entry->value->get_stack_size() != 1)
            error_code = FT_ERR_INVALID_STATE;
        else if (this->_revision == std::numeric_limits<uint64_t>::max())
            error_code = FT_ERR_OUT_OF_RANGE;
        else
        {
            this->_current_weight -= entry->value->get_stack_size();
#if USE_INVENTORY_SLOTS
            const int32_t area = entry->value->get_width()
                * entry->value->get_height();
            if (area >= 0 && this->_used_slots >= static_cast<ft_size_t>(area))
                this->_used_slots -= static_cast<ft_size_t>(area);
#endif
            this->_items.remove(slot);
            error_code = this->bump_revision();
        }
    }
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_inventory::remove_item_quantity(int32_t item_id,
    int32_t quantity, uint64_t expected_revision,
    uint64_t transaction_scope_id, uint64_t transaction_id,
    game_inventory_quantity_result *result_out) noexcept
{
    typedef std::pair<uint64_t, uint64_t> transaction_key;
    std::map<transaction_key, QuantityRemoveCacheEntry>::iterator cached;
    QuantityRemoveCacheEntry staged = {};
    game_inventory_quantity_result result = {};
    int64_t available = 0;
    uint64_t slots_to_release = 0U;
    int32_t preflight_remaining = quantity;
    int32_t error_code = FT_ERR_SUCCESS;
    ft_bool lock_acquired = FT_FALSE;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::remove_item_quantity");
    if (result_out == nullptr || item_id <= 0 || quantity <= 0
        || transaction_scope_id == 0U || transaction_id == 0U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    cached = this->_quantity_remove_cache.find(
        transaction_key(transaction_scope_id, transaction_id));
    if (cached == this->_quantity_remove_cache.end()
        && this->_transfer_cache.find(transaction_key(transaction_scope_id,
            transaction_id)) != this->_transfer_cache.end())
    {
        (void)this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    if (cached != this->_quantity_remove_cache.end())
    {
        if (cached->second.item_id != item_id
            || cached->second.quantity != quantity
            || cached->second.expected_revision != expected_revision)
            error_code = FT_ERR_INVALID_ARGUMENT;
        else
        {
            *result_out = cached->second.result;
            error_code = cached->second.result.result_code;
        }
        (void)this->unlock_internal(lock_acquired);
        return (this->set_error(error_code));
    }
    if (this->_quantity_remove_cache.size() >= TRANSFER_CACHE_LIMIT)
    {
        (void)this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_FULL));
    }
    staged.item_id = item_id;
    staged.quantity = quantity;
    staged.expected_revision = expected_revision;
    result.transaction_scope_id = transaction_scope_id;
    result.transaction_id = transaction_id;
    result.inventory_revision = this->_revision;
    result.item_id = item_id;
    result.requested_quantity = quantity;
    result.removed_quantity = 0;
    result.result_code = FT_ERR_INVALID_STATE;
    staged.result = result;
    try
    {
        cached = this->_quantity_remove_cache.emplace(
            transaction_key(transaction_scope_id, transaction_id),
            staged).first;
    }
    catch (const std::bad_alloc &)
    {
        (void)this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    if (this->_revision != expected_revision)
        error_code = FT_ERR_INVALID_STATE;
    else
    {
        int32_t slot = 0;

        while (slot < this->_next_slot)
        {
            Pair<int32_t, ft_sharedptr<game_item> > *entry =
                this->_items.find(slot);
            if (entry != this->_items.end() && entry->value
                && entry->value->get_item_id() == item_id
                && entry->value->get_instance_id() == 0U)
            {
                available += entry->value->get_stack_size();
                if (preflight_remaining > 0)
                {
                    const int32_t stack_size =
                        entry->value->get_stack_size();
                    if (stack_size <= preflight_remaining)
                    {
#if USE_INVENTORY_SLOTS
                        const int64_t area =
                            static_cast<int64_t>(entry->value->get_width())
                            * entry->value->get_height();
                        if (area > 0)
                            slots_to_release +=
                                static_cast<uint64_t>(area);
#endif
                        preflight_remaining -= stack_size;
                    }
                    else
                        preflight_remaining = 0;
                }
            }
            ++slot;
        }
        if (available < quantity)
            error_code = FT_ERR_NOT_FOUND;
        else if (this->_current_weight < quantity
#if USE_INVENTORY_SLOTS
            || slots_to_release > this->_used_slots
#endif
            )
            error_code = FT_ERR_INVALID_STATE;
        else if (this->_revision == std::numeric_limits<uint64_t>::max())
            error_code = FT_ERR_OUT_OF_RANGE;
        else
        {
            int32_t remaining = quantity;
            slot = 0;
            while (slot < this->_next_slot && remaining > 0)
            {
                Pair<int32_t, ft_sharedptr<game_item> > *entry =
                    this->_items.find(slot);
                if (entry != this->_items.end() && entry->value
                    && entry->value->get_item_id() == item_id
                    && entry->value->get_instance_id() == 0U)
                {
                    const int32_t stack = entry->value->get_stack_size();
                    const int32_t taken = stack < remaining ? stack : remaining;

                    if (taken == stack)
                    {
#if USE_INVENTORY_SLOTS
                        const int64_t area =
                            static_cast<int64_t>(entry->value->get_width())
                            * entry->value->get_height();
                        if (area >= 0
                            && static_cast<uint64_t>(area) <= this->_used_slots)
                            this->_used_slots -=
                                static_cast<ft_size_t>(area);
#endif
                        this->_items.remove(slot);
                    }
                    else
                        entry->value->set_stack_size(stack - taken);
                    remaining -= taken;
                }
                ++slot;
            }
            this->_current_weight -= quantity;
            error_code = this->bump_revision();
            result.removed_quantity = quantity;
        }
    }
    result.inventory_revision = this->_revision;
    result.result_code = error_code;
    cached->second.result = result;
    *result_out = result;
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
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
