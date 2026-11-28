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
#include "../Template/unordered_map.hpp"
#include "../Template/vector.hpp"
#include <algorithm>
#include <functional>
#include <limits>

thread_local int32_t game_inventory::_last_error = FT_ERR_SUCCESS;

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
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_inventory::initialize(copy)", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (this->set_error(FT_ERR_OUT_OF_RANGE));

    result = this->_items.destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->_items.initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool source_lock_acquired = FT_FALSE;
    result = other.lock_internal(&source_lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_items.copy_from(other._items);
        if (result == FT_ERR_SUCCESS)
        {
            this->_capacity = other._capacity;
            this->_used_slots = other._used_slots;
            this->_weight_limit = other._weight_limit;
            this->_current_weight = other._current_weight;
            this->_next_slot = other._next_slot;
        }
        (void)other.unlock_internal(source_lock_acquired);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_items.destroy();
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)this->bump_revision();
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_inventory::initialize(game_inventory &&other) noexcept
{
    return (this->move(other));
}

int32_t game_inventory::move(game_inventory &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_inventory::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (other._mutex != ft_nullptr && other._mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));

    result = this->_items.destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool source_lock_acquired = FT_FALSE;
    result = other.lock_internal(&source_lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    if (other._revision == std::numeric_limits<uint64_t>::max())
        result = FT_ERR_OUT_OF_RANGE;
    else
        result = this->_items.move(other._items);
    if (result == FT_ERR_SUCCESS)
    {
        this->_capacity = other._capacity;
        this->_used_slots = other._used_slots;
        this->_weight_limit = other._weight_limit;
        this->_current_weight = other._current_weight;
        this->_next_slot = other._next_slot;
    }
    (void)other.unlock_internal(source_lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_items.destroy();
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)this->bump_revision();
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
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
                result = entry->value->capture_inventory_fields(entry->key, snapshot);
                if (result != FT_ERR_SUCCESS)
                    break ;
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
    ft_unordered_map<uint64_t, ft_bool> instance_ids;
    int64_t calculated_weight = 0;
    uint64_t calculated_slots = 0U;
    int32_t maximum_slot = -1;
    int32_t result;
    ft_bool slot_total_overflow = FT_FALSE;
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
    constexpr ft_size_t DIRECT_ID_SCAN_LIMIT = 16U;
    auto entry_iterator = snapshot.items.begin();
    while (entry_iterator != snapshot.items.end())
    {
        const game_inventory_item_snapshot &entry = *entry_iterator;
        if (entry.slot < 0 || entry.slot >= snapshot.next_slot
            || entry.item_id <= 0 || entry.stack_size <= 0
            || entry.max_stack <= 0 || entry.stack_size > entry.max_stack
            || entry.width <= 0 || entry.height <= 0
            || (entry.instance_id != 0U
                && (entry.stack_size != 1 || entry.max_stack != 1)))
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        if (entry.slot <= maximum_slot
            && staged_inventory._items.find(entry.slot) != staged_inventory._items.end())
            return (this->set_error(FT_ERR_ALREADY_EXISTS));
        if (entry.instance_id != 0U)
        {
            if (snapshot.items.size() <= DIRECT_ID_SCAN_LIMIT)
            {
                auto prior = snapshot.items.begin();
                while (prior != entry_iterator)
                {
                    if (prior->instance_id == entry.instance_id)
                        return (this->set_error(FT_ERR_ALREADY_EXISTS));
                    ++prior;
                }
            }
            else
            {
                if (instance_ids.is_initialised() != FT_CLASS_STATE_INITIALISED)
                {
                    result = instance_ids.initialize();
                    if (result != FT_ERR_SUCCESS)
                        return (this->set_error(result));
                }
                const ft_size_t previous_count = instance_ids.size();
                instance_ids.insert(entry.instance_id, FT_TRUE);
                result = instance_ids.get_error();
                if (result != FT_ERR_SUCCESS)
                    return (this->set_error(result));
                if (instance_ids.size() == previous_count)
                    return (this->set_error(FT_ERR_ALREADY_EXISTS));
            }
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
        staged_inventory._items.insert(entry.slot, item);
        result = staged_inventory._items.get_error();
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        if (calculated_weight <= std::numeric_limits<int32_t>::max())
            calculated_weight += static_cast<int64_t>(entry.stack_size);
        const uint64_t slot_area = static_cast<uint64_t>(entry.width)
            * static_cast<uint64_t>(entry.height);
        if (slot_area > UINT64_MAX - calculated_slots)
            slot_total_overflow = FT_TRUE;
        else
            calculated_slots += slot_area;
        if (entry.slot > maximum_slot)
            maximum_slot = entry.slot;
        ++entry_iterator;
    }
    if (calculated_weight != snapshot.current_weight
        || calculated_weight > std::numeric_limits<int32_t>::max()
        || slot_total_overflow == FT_TRUE
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

static int32_t clone_transfer_items(
    ft_map<int32_t, ft_sharedptr<game_item> > &staged_items,
    const ft_map<int32_t, ft_sharedptr<game_item> > &live_items)
{
    int32_t result = staged_items.copy_from(live_items);
    if (result != FT_ERR_SUCCESS)
        return (result);

    // Copy the map in its existing order, then detach every live item before mutation.
    auto *end = staged_items.end();
    auto *entry = end - staged_items.size();
    while (entry != end)
    {
        if (entry->value)
        {
            ft_sharedptr<game_item> copy(new (std::nothrow) game_item());
            if (!copy)
                return (FT_ERR_NO_MEMORY);
            result = copy->initialize(*entry->value);
            if (result == FT_ERR_SUCCESS)
                result = entry->value.move(copy);
            if (result != FT_ERR_SUCCESS)
                return (result);
        }
        ++entry;
    }
    return (FT_ERR_SUCCESS);
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
    using item_map = ft_map<int32_t, ft_sharedptr<game_item> >;
    item_map staged_source;
    item_map staged_destination;
    item_map backup_destination(0U);
    ft_size_t destination_used_slots;
    int32_t destination_next_slot;
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
    destination_used_slots = destination._used_slots;
    destination_next_slot = destination._next_slot;
    result = clone_transfer_items(staged_source, this->_items);
    if (result == FT_ERR_SUCCESS)
        result = clone_transfer_items(staged_destination, destination._items);
    if (result != FT_ERR_SUCCESS)
        goto transfer_cleanup;
    source_entry = staged_source.find(source_slot);
    if (source_entry == staged_source.end() || !source_entry->value)
    {
        result = FT_ERR_INTERNAL;
        goto transfer_cleanup;
    }
    /* Debit/remove only in the staged source copy. */
    if (quantity == source_stack)
    {
        result = moved_item.move(source_entry->value);
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        staged_source.remove(source_slot);
        result = staged_source.get_error();
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
    }
    else
    {
        result = moved_item.initialize(new (std::nothrow) game_item());
        if (result != FT_ERR_SUCCESS || !moved_item)
        {
            if (result == FT_ERR_SUCCESS)
                result = FT_ERR_NO_MEMORY;
            goto transfer_cleanup;
        }
        result = moved_item->initialize(*source_entry->value);
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        moved_item->set_stack_size(quantity);
        source_entry->value->set_stack_size(source_stack - quantity);
    }

    /* Merge only completely compatible item metadata. */
    remaining = requested_quantity;
    entry_count = staged_destination.size();
    entry = entry_count == 0U ? ft_nullptr
        : staged_destination.end() - entry_count;
    index = 0U;
    while (index < entry_count && remaining > 0)
    {
        if (entry->value)
        {
            int32_t merged = 0;
            result = entry->value->merge_stack_from(*moved_item, remaining, merged);
            if (result != FT_ERR_SUCCESS)
                goto transfer_cleanup;
            remaining -= merged;
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
        ft_sharedptr<game_item> stack;

#if USE_INVENTORY_SLOTS
        if (destination._capacity != 0
            && (destination_used_slots > destination._capacity
                || static_cast<ft_size_t>(slot_usage)
                    > destination._capacity - destination_used_slots))
        {
            result = FT_ERR_FULL;
            goto transfer_cleanup;
        }
        if (static_cast<ft_size_t>(slot_usage)
            > std::numeric_limits<ft_size_t>::max()
                - destination_used_slots)
        {
            result = FT_ERR_OUT_OF_RANGE;
            goto transfer_cleanup;
        }
#endif
        if (destination_next_slot == FT_INT32_MAX)
        {
            result = FT_ERR_OUT_OF_RANGE;
            goto transfer_cleanup;
        }
        if (amount == remaining)
            result = stack.move(moved_item);
        else
        {
            result = stack.initialize(new (std::nothrow) game_item());
            if (result == FT_ERR_SUCCESS && !stack)
                result = FT_ERR_NO_MEMORY;
            if (result == FT_ERR_SUCCESS)
                result = stack->initialize(*moved_item);
        }
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        stack->set_stack_size(amount);
        staged_destination.insert(destination_next_slot, stack);
        result = staged_destination.get_error();
        if (result != FT_ERR_SUCCESS)
            goto transfer_cleanup;
        destination_next_slot += 1;
#if USE_INVENTORY_SLOTS
        destination_used_slots += static_cast<ft_size_t>(slot_usage);
#endif
        remaining -= amount;
    }

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
    result = backup_destination.initialize();
    if (result == FT_ERR_SUCCESS)
        result = backup_destination.move_from(destination._items);
    if (result == FT_ERR_SUCCESS)
        destination_backup_ready = FT_TRUE;
    if (result == FT_ERR_SUCCESS)
        result = destination._items.move_from(staged_destination);
    if (result == FT_ERR_SUCCESS)
        result = this->_items.move_from(staged_source);
    if (result != FT_ERR_SUCCESS && destination_backup_ready == FT_TRUE)
    {
        const int32_t rollback_result = destination._items.move_from(
            backup_destination);

        if (rollback_result != FT_ERR_SUCCESS)
            result = FT_ERR_INTERNAL;
    }
    if (result == FT_ERR_SUCCESS)
    {
#if USE_INVENTORY_SLOTS
        if (quantity == source_stack)
            this->_used_slots -= static_cast<ft_size_t>(area);
#endif
        this->_current_weight -= quantity;
        destination._used_slots = destination_used_slots;
        destination._current_weight += requested_quantity;
        destination._next_slot = destination_next_slot;
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

int32_t game_inventory::restore_serialized_item(
    const ft_sharedptr<game_item> &item) noexcept
{
    const int32_t quantity = item->get_stack_size();
    const int32_t max_stack = item->get_max_stack();
    if (quantity > 0 && max_stack <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (quantity <= 0 || quantity > max_stack)
        return (this->add_item(item));

    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    if (this->_revision == std::numeric_limits<uint64_t>::max())
    {
        (void)this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    if (quantity > 1)
    {
        const auto *end = this->_items.end();
        const auto *entry = end - this->_items.size();
        while (entry != end)
        {
            if (entry->value)
            {
                int32_t room = 0;
                const int32_t result = entry->value->stack_room_for(*item, room);
                if (result != FT_ERR_SUCCESS || room > 0)
                {
                    (void)this->unlock_internal(lock_acquired);
                    if (result != FT_ERR_SUCCESS)
                        return (this->set_error(result));
                    return (this->add_item(item));
                }
            }
            ++entry;
        }
    }
    // A single merge or insertion cannot leave a partially applied row.
    int32_t result = this->add_item_in_place(item);
    if (result == FT_ERR_SUCCESS)
        result = this->bump_revision();
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_inventory::add_item_in_place(
    const ft_sharedptr<game_item> &item) noexcept
{
    int32_t remaining;
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
    if (remaining > 0 && this->_current_weight > INT32_MAX - remaining)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
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
            int32_t merged = 0;
            const int32_t result = item_pointer->value->merge_stack_from(*item, remaining, merged);
            if (result != FT_ERR_SUCCESS)
                return (this->set_error(result));
            this->_current_weight += merged;
            remaining -= merged;
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

int32_t game_inventory::erase_slot_unlocked(int32_t slot, uint64_t instance_id) noexcept
{
    using item_entry = Pair<int32_t, ft_sharedptr<game_item> >;
    int32_t weight_after = this->_current_weight;
#if USE_INVENTORY_SLOTS
    ft_size_t slots_to_release = 0U;
#endif
    const int32_t result = this->_items.erase_value(slot,
        [&](const ft_sharedptr<game_item> &handle) noexcept -> int32_t
        {
            if (instance_id == 0U && this->_revision == std::numeric_limits<uint64_t>::max())
                return (FT_ERR_OUT_OF_RANGE);
            const game_item *item = handle.get();
            const int32_t handle_error = handle.get_error();
            if (handle_error != FT_ERR_SUCCESS)
                return (handle_error);
            if (item == ft_nullptr && instance_id != 0U)
                return (FT_ERR_NOT_FOUND);
            if (item != ft_nullptr)
            {
                ft_bool item_locked = FT_FALSE;
                const int32_t error = item->lock_internal(&item_locked);
                if (error != FT_ERR_SUCCESS)
                    return (error);
                const int32_t stack_size = item->_stack_size;
                const uint64_t actual_instance_id = item->_instance_id;
#if USE_INVENTORY_SLOTS
                const int64_t area = static_cast<int64_t>(item->_width) * item->_height;
#endif
                item->unlock_internal(item_locked);
                if (instance_id != 0U && (actual_instance_id != instance_id || stack_size != 1))
                    return (FT_ERR_INVALID_STATE);
                const int64_t remaining_weight = static_cast<int64_t>(this->_current_weight) - stack_size;
                if (remaining_weight < INT32_MIN || remaining_weight > INT32_MAX)
                    return (FT_ERR_OUT_OF_RANGE);
                weight_after = static_cast<int32_t>(remaining_weight);
#if USE_INVENTORY_SLOTS
                if (area >= 0 && static_cast<uint64_t>(area) <= this->_used_slots)
                    slots_to_release = static_cast<ft_size_t>(area);
#endif
            }
            if (instance_id != 0U && this->_revision == std::numeric_limits<uint64_t>::max())
                return (FT_ERR_OUT_OF_RANGE);
            return (FT_ERR_SUCCESS);
        },
        [](item_entry &destination, item_entry &source) noexcept
        {
            destination.key = source.key;
            destination.value.swap(source.value);
            return ;
        });
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_current_weight = weight_after;
#if USE_INVENTORY_SLOTS
    this->_used_slots -= slots_to_release;
#endif
    return (this->bump_revision());
}

void game_inventory::remove_item(int32_t slot) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::remove_item");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return ;
    result = this->erase_slot_unlocked(slot, 0U);
    (void)this->unlock_internal(lock_acquired);
    if (result == FT_ERR_NOT_FOUND)
        result = FT_ERR_SUCCESS;
    this->set_error(result);
    return ;
}

int32_t game_inventory::remove_item_instance(int32_t slot,
    uint64_t expected_revision, uint64_t instance_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_inventory::remove_item_instance");
    if (slot < 0 || instance_id == 0U)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (this->_revision != expected_revision)
        result = FT_ERR_INVALID_STATE;
    else
        result = this->erase_slot_unlocked(slot, instance_id);
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_inventory::remove_quantity_in_place(int32_t item_id, int32_t quantity) noexcept
{
    struct removal_entry
    {
        ft_size_t index;
        int32_t slot;
        int32_t stack;
        int64_t area;
    };
    ft_vector<removal_entry> plan;
    int32_t result = plan.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    auto *end = this->_items.end();
    auto *first = end - this->_items.size();
    const auto *entry = first;
    int64_t available = 0;
    int32_t previous_slot = -1;
    ft_bool ascending = FT_TRUE;
    while (entry != end)
    {
        if (entry->key >= 0 && entry->key < this->_next_slot && entry->value)
        {
            const game_item &item = *entry->value.get();
            ft_bool item_locked = FT_FALSE;
            result = item.lock_internal(&item_locked);
            if (result != FT_ERR_SUCCESS)
                return (result);
            const ft_bool matches = item._item_id == item_id && item._instance_id == 0U;
            const int32_t stack = item._stack_size;
            const int64_t area = static_cast<int64_t>(item._width) * item._height;
            item.unlock_internal(item_locked);
            if (matches == FT_TRUE)
            {
                if (stack < 0)
                    return (FT_ERR_INVALID_STATE);
                if (entry->key < previous_slot)
                    ascending = FT_FALSE;
                previous_slot = entry->key;
                result = plan.push_back(removal_entry{static_cast<ft_size_t>(entry - first),
                    entry->key, stack, area});
                if (result != FT_ERR_SUCCESS)
                    return (result);
                available += stack;
            }
        }
        ++entry;
    }
    if (available < quantity)
        return (FT_ERR_NOT_FOUND);
    if (this->_current_weight < quantity)
        return (FT_ERR_INVALID_STATE);

    removal_entry *next = plan.begin();
    if (ascending == FT_FALSE)
        std::sort(next, plan.end(), [](const removal_entry &left, const removal_entry &right)
        {
            return (left.slot < right.slot);
        });
    removal_entry *full_end = next;
    int32_t remaining = quantity;
#if USE_INVENTORY_SLOTS
    ft_size_t slots_to_release = 0U;
#endif
    while (remaining > 0 && full_end->stack <= remaining)
    {
#if USE_INVENTORY_SLOTS
        if (full_end->area > 0)
        {
            if (static_cast<uint64_t>(full_end->area) > this->_used_slots - slots_to_release)
                return (FT_ERR_INVALID_STATE);
            slots_to_release += static_cast<ft_size_t>(full_end->area);
        }
#endif
        remaining -= full_end->stack;
        ++full_end;
    }
    if (this->_revision == std::numeric_limits<uint64_t>::max())
        return (FT_ERR_OUT_OF_RANGE);

    game_item *partial_item = ft_nullptr;
    ft_bool partial_locked = FT_FALSE;
    if (remaining > 0)
    {
        partial_item = first[full_end->index].value.get();
        result = partial_item->lock_internal(&partial_locked);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    if (next != full_end)
    {
        if (ascending == FT_FALSE)
            std::sort(next, full_end, [](const removal_entry &left, const removal_entry &right)
            {
                return (left.index < right.index);
            });
        using item_entry = Pair<int32_t, ft_sharedptr<game_item> >;
        result = this->_items.erase_if(
            [&next, full_end](const item_entry &, ft_size_t index) noexcept -> ft_bool
            {
                if (next == full_end || next->index != index)
                    return (FT_FALSE);
                ++next;
                return (FT_TRUE);
            },
            [](item_entry &target, item_entry &source) noexcept
            {
                target.key = source.key;
                (void)target.value.move(source.value);
                return ;
            });
    }
    if (result == FT_ERR_SUCCESS)
    {
        if (partial_item != ft_nullptr)
            partial_item->_stack_size = full_end->stack - remaining;
        this->_current_weight -= quantity;
#if USE_INVENTORY_SLOTS
        this->_used_slots -= slots_to_release;
#endif
        result = this->bump_revision();
    }
    if (partial_item != ft_nullptr)
        partial_item->unlock_internal(partial_locked);
    return (result);
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
        ft_bool map_locked = FT_FALSE;
        error_code = this->_items.lock(&map_locked);
        if (error_code == FT_ERR_SUCCESS)
        {
            error_code = this->remove_quantity_in_place(item_id, quantity);
            (void)this->_items.unlock(map_locked);
        }
        if (error_code == FT_ERR_SUCCESS)
            result.removed_quantity = quantity;
    }
    result.inventory_revision = this->_revision;
    result.result_code = error_code;
    cached->second.result = result;
    *result_out = result;
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_inventory::count_by_item_field(int32_t game_item::*field,
    int32_t value) const noexcept
{
    ft_bool inventory_locked = FT_FALSE;
    int32_t result = this->lock_internal(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0);
    }
    int64_t total = 0;
    using item_entry = Pair<int32_t, ft_sharedptr<game_item> >;
    result = this->_items.visit_entries(
        [this, field, value, &total](const item_entry &entry) noexcept -> int32_t
        {
            if (entry.key < 0 || entry.key >= this->_next_slot)
                return (FT_ERR_SUCCESS);
            const game_item *item = entry.value.get();
            if (item == ft_nullptr)
                return (FT_ERR_SUCCESS);
            ft_bool item_locked = FT_FALSE;
            if (item->_mutex != ft_nullptr)
            {
                const int32_t lock_error = item->lock_internal(&item_locked);
                if (lock_error != FT_ERR_SUCCESS)
                    return (lock_error);
            }
            if (item->*field == value)
                total += item->_stack_size;
            if (item_locked == FT_TRUE)
                item->unlock_internal(item_locked);
            return (FT_ERR_SUCCESS);
        });
    const int32_t unlock_error = this->unlock_internal(inventory_locked);
    if (result == FT_ERR_SUCCESS)
        result = unlock_error;
    if (result == FT_ERR_SUCCESS && (total < INT32_MIN || total > INT32_MAX))
        result = FT_ERR_OUT_OF_RANGE;
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    return (static_cast<int32_t>(total));
}

int32_t game_inventory::count_item(int32_t item_id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::count_item");
    return (this->count_by_item_field(&game_item::_item_id, item_id));
}

ft_bool game_inventory::has_item(int32_t item_id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::has_item");
    return (this->count_by_item_field(&game_item::_item_id, item_id) > 0);
}

int32_t game_inventory::count_rarity(int32_t rarity) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::count_rarity");
    return (this->count_by_item_field(&game_item::_rarity, rarity));
}

ft_bool game_inventory::has_rarity(int32_t rarity) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_inventory::has_rarity");
    return (this->count_by_item_field(&game_item::_rarity, rarity) > 0);
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
