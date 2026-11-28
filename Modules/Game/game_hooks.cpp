#include "../PThread/pthread_internal.hpp"
#include "game_hooks.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
 #include "../Template/move.hpp"
 #include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_crafting.hpp"
#include "game_currency_rate.hpp"
#include "game_debuff.hpp"
#include "game_dialogue_line.hpp"
#include "game_dialogue_script.hpp"
#include "game_dialogue_table.hpp"
#include "game_economy_table.hpp"
#include "game_pathfinding.hpp"
#include "game_price_definition.hpp"
#include "game_quest.hpp"
#include "game_rarity_band.hpp"
#include "game_region_definition.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"
#include "game_vendor_profile.hpp"
#include "game_world_region.hpp"
#include "game_world_registry.hpp"
#include "game_world_replay.hpp"

thread_local int32_t game_hooks::_last_error = FT_ERR_SUCCESS;

int32_t game_hooks::set_error(int32_t error_code) noexcept
{
    game_hooks::_last_error = error_code;
    return (error_code);
}

int32_t game_hooks::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_hooks::get_error");
    return (game_hooks::_last_error);
}

const char *game_hooks::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_hooks::get_error_str");
    return (ft_strerror(game_hooks::_last_error));
}

const char *ft_game_hook_item_crafted_identifier = "game.item.crafted";
const char *ft_game_hook_character_damaged_identifier = "game.character.damaged";
const char *ft_game_hook_event_triggered_identifier = "game.event.triggered";

int32_t ft_game_hook_metadata::initialize(
    const ft_game_hook_metadata &other) noexcept
{
    int32_t error_code;

    error_code = this->hook_identifier.initialize(other.hook_identifier);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->listener_name.initialize(other.listener_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->description.initialize(other.description);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->argument_contract.initialize(other.argument_contract);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t ft_game_hook_metadata::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->hook_identifier.destroy();
    error_code = this->listener_name.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->description.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    error_code = this->argument_contract.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    return (first_error);
}

int32_t ft_game_hook_metadata::initialize(ft_game_hook_metadata &&other) noexcept
{
    int32_t result = this->hook_identifier.move(other.hook_identifier);
    if (result == FT_ERR_SUCCESS)
        result = this->listener_name.move(other.listener_name);
    if (result == FT_ERR_SUCCESS)
        result = this->description.move(other.description);
    if (result == FT_ERR_SUCCESS)
        result = this->argument_contract.move(other.argument_contract);
    return (result);
}

int32_t ft_game_hook_listener_entry::initialize(
    const ft_game_hook_listener_entry &other) noexcept
{
    int32_t error_code;

    error_code = this->metadata.initialize(other.metadata);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->priority = other.priority;
    this->callback = other.callback;
    if (other.callback && !this->callback)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t ft_game_hook_listener_entry::destroy() noexcept
{
    int32_t error_code;

    error_code = this->metadata.destroy();
    this->priority = 0;
    this->callback = ft_function<void(ft_game_hook_context&)>();
    return (error_code);
}

int32_t ft_game_hook_listener_entry::initialize(ft_game_hook_listener_entry &&other) noexcept
{
    const int32_t result = this->metadata.initialize(ft_move(other.metadata));
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->priority = other.priority;
    this->callback = ft_move(other.callback);
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::listener_record::initialize(const listener_record &other) noexcept
{
    const int32_t result = this->metadata.initialize(other.metadata);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->priority = other.priority;
    return (this->callback.initialize(other.callback));
}

int32_t game_hooks::listener_record::initialize(listener_record &&other) noexcept
{
    const int32_t result = this->metadata.initialize(ft_move(other.metadata));
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->priority = other.priority;
    return (this->callback.move(other.callback));
}

int32_t game_hooks::listener_record::set_callback(hook_callback &&function) noexcept
{
    callback_owner staged;
    hook_callback *stored = ft_nullptr;
    if (function)
    {
        stored = new (std::nothrow) hook_callback(ft_move(function));
        if (stored == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    const int32_t result = staged.initialize(stored);
    if (result != FT_ERR_SUCCESS)
    {
        delete stored;
        return (result);
    }
    return (this->callback.move(staged));
}

int32_t game_hooks::listener_record::clone_callback() noexcept
{
    const hook_callback *original = this->callback.get();
    if (original == ft_nullptr)
        return (FT_ERR_SUCCESS);
    hook_callback copy(*original);
    if (!copy)
        return (FT_ERR_NO_MEMORY);
    return (this->set_callback(ft_move(copy)));
}

static int32_t game_hooks_initialize_metadata(ft_game_hook_metadata &metadata,
    const char *hook_identifier, const char *listener_name,
    const char *description, const char *argument_contract) noexcept
{
    int32_t error_code;

    error_code = metadata.hook_identifier.initialize(hook_identifier);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.listener_name.initialize(listener_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.description.initialize(description);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.argument_contract.initialize(argument_contract);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

ft_function<void(ft_game_hook_context&)> ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)> &&callback) noexcept
{
    return (ft_function<void(ft_game_hook_context&)>([inner_callback = ft_move(callback)](ft_game_hook_context &context) mutable
    {
        if (!inner_callback)
            return ;
        if (context.character == ft_nullptr || context.item == ft_nullptr)
            return ;
        inner_callback(*context.character, *context.item);
        return ;
    }));
}

ft_function<void(ft_game_hook_context&)> ft_game_hook_make_character_damage_adapter(ft_function<void(game_character&, int32_t, uint8_t)> &&callback) noexcept
{
    return (ft_function<void(ft_game_hook_context&)>([inner_callback = ft_move(callback)](ft_game_hook_context &context) mutable
    {
        if (!inner_callback)
            return ;
        if (context.character == ft_nullptr)
            return ;
        inner_callback(*context.character, context.integer_payload, context.small_payload);
        return ;
    }));
}

ft_function<void(ft_game_hook_context&)> ft_game_hook_make_world_event_adapter(ft_function<void(game_world&, game_event&)> &&callback) noexcept
{
    return (ft_function<void(ft_game_hook_context&)>([inner_callback = ft_move(callback)](ft_game_hook_context &context) mutable
    {
        if (!inner_callback)
            return ;
        if (context.world == ft_nullptr || context.event == ft_nullptr)
            return ;
        inner_callback(*context.world, *context.event);
        return ;
    }));
}

game_hooks::game_hooks() noexcept
    : _legacy_item_crafted(), _legacy_character_damaged(), _legacy_event_triggered(),
      _listener_catalog(), _catalog_metadata(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_hooks::~game_hooks() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_hooks::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_hooks::initialize",
            "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_legacy_item_crafted = ft_function<void(game_character&, game_item&)>();
    this->_legacy_character_damaged = ft_function<void(game_character&, int32_t, uint8_t)>();
    this->_legacy_event_triggered = ft_function<void(game_world&, game_event&)>();
    int32_t map_error = this->_listener_catalog.initialize();
    if (map_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(map_error);
        return (map_error);
    }
    int32_t metadata_error = this->_catalog_metadata.initialize();
    if (metadata_error != FT_ERR_SUCCESS)
    {
        (void)this->_listener_catalog.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(metadata_error);
        return (metadata_error);
    }
    this->_listener_catalog.clear();
    this->_catalog_metadata.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::initialize(const game_hooks &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    ft_bool source_lock_acquired;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_hooks::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = other.lock_internal(&source_lock_acquired);
    if (initialize_error == FT_ERR_SUCCESS)
    {
        initialize_error = this->_listener_catalog.copy_from(other._listener_catalog);
        if (initialize_error == FT_ERR_SUCCESS)
            initialize_error = this->clone_listener_callbacks_unlocked();
        if (initialize_error == FT_ERR_SUCCESS)
            initialize_error = this->_catalog_metadata.replace_from(other._catalog_metadata);
        if (initialize_error == FT_ERR_SUCCESS)
        {
            this->_legacy_item_crafted = other._legacy_item_crafted;
            this->_legacy_character_damaged = other._legacy_character_damaged;
            this->_legacy_event_triggered = other._legacy_event_triggered;
        }
        (void)other.unlock_internal(source_lock_acquired);
    }
    if (initialize_error != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(initialize_error));
}

int32_t game_hooks::clone_listener_callbacks_unlocked() noexcept
{
    const ft_size_t count = this->_listener_catalog.size();
    if (count == 0U)
        return (FT_ERR_SUCCESS);
    auto *end = this->_listener_catalog.end();
    auto *entry = end - count;
    while (entry != end)
    {
        const ft_size_t listener_count = entry->value.size();
        listener_record *listeners = entry->value.begin();
        ft_size_t index = 0U;
        while (index < listener_count)
        {
            const int32_t result = listeners[index].clone_callback();
            if (result != FT_ERR_SUCCESS)
                return (result);
            ++index;
        }
        ++entry;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::initialize(game_hooks &&other) noexcept
{
    return (this->move(other));
}

int32_t game_hooks::move(game_hooks &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_hooks::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_mutex != ft_nullptr && this->_mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));
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
    if (other._mutex != ft_nullptr && other._mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    // The map move initializes the destination with the source's storage.
    result = this->_catalog_metadata.initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    // The owned containers have no separate mutexes. Release the optional
    // registry mutex before committing the storage transfer.
    result = other.disable_thread_safety();
    if (result == FT_ERR_SUCCESS)
        result = this->_listener_catalog.move(other._listener_catalog);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->move_catalog_metadata_unlocked(other);
        if (result != FT_ERR_SUCCESS)
            (void)other._listener_catalog.move(this->_listener_catalog);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->_legacy_item_crafted = ft_move(other._legacy_item_crafted);
    this->_legacy_character_damaged = ft_move(other._legacy_character_damaged);
    this->_legacy_event_triggered = ft_move(other._legacy_event_triggered);
    return (this->set_error(other.destroy()));
}

int32_t game_hooks::move_catalog_metadata_unlocked(game_hooks &other) noexcept
{
    const ft_size_t count = other._catalog_metadata.size();
    if (count > this->_catalog_metadata.capacity())
        return (this->_catalog_metadata.move(other._catalog_metadata));
    // Each record owns initialized strings without per-string mutexes.
    // Transfer their buffers directly when the destination has inline space.
    ft_game_hook_metadata *metadata = other._catalog_metadata.begin();
    ft_size_t index = 0U;
    while (index < count)
    {
        const int32_t result = this->_catalog_metadata.push_back(ft_move(metadata[index]));
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_legacy_item_crafted = ft_function<void(game_character&, game_item&)>();
    this->_legacy_character_damaged = ft_function<void(game_character&, int32_t, uint8_t)>();
    this->_legacy_event_triggered = ft_function<void(game_world&, game_event&)>();
    int32_t map_destroy_error = this->_listener_catalog.destroy();
    int32_t metadata_destroy_error = this->_catalog_metadata.destroy();
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    int32_t final_error = disable_error;
    if (final_error == FT_ERR_SUCCESS && map_destroy_error != FT_ERR_SUCCESS)
        final_error = map_destroy_error;
    if (final_error == FT_ERR_SUCCESS && metadata_destroy_error != FT_ERR_SUCCESS)
        final_error = metadata_destroy_error;
    this->set_error(final_error);
    return (final_error);
}

int32_t game_hooks::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::enable_thread_safety");
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

int32_t game_hooks::disable_thread_safety() noexcept
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

ft_bool game_hooks::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_hooks::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_hooks::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::lock");
    int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_hooks::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void game_hooks::remove_listener_unlocked(const ft_string &hook_identifier,
    const ft_string &listener_name) noexcept
{
    Pair<ft_string, ft_vector<listener_record> > *listeners_pair;
    ft_size_t index;

    listeners_pair = this->_listener_catalog.find(hook_identifier);
    if (listeners_pair == this->_listener_catalog.end())
        return ;
    index = 0;
    while (index < listeners_pair->value.size())
    {
        if (listeners_pair->value[index].metadata.listener_name == listener_name)
        {
            listeners_pair->value.erase(listeners_pair->value.begin() + index);
            break ;
        }
        index += 1;
    }
    index = this->find_metadata_index_unlocked(hook_identifier, listener_name);
    if (index != this->_catalog_metadata.size())
        this->_catalog_metadata.erase(this->_catalog_metadata.begin() + index);
    return ;
}

ft_size_t game_hooks::find_metadata_index_unlocked(const ft_string &hook_identifier,
    const ft_string &listener_name) const noexcept
{
    const ft_size_t count = this->_catalog_metadata.size();
    const ft_game_hook_metadata *metadata = this->_catalog_metadata.begin();
    ft_size_t index = 0U;
    while (index < count)
    {
        if (metadata[index].hook_identifier == hook_identifier
            && metadata[index].listener_name == listener_name)
            return (index);
        ++index;
    }
    return (count);
}

int32_t game_hooks::insert_callback_unlocked(
    listener_record &entry, ft_bool &replaced) noexcept
{
    auto *listeners_pair = this->_listener_catalog.find(entry.metadata.hook_identifier);
    if (listeners_pair == this->_listener_catalog.end())
    {
        ft_vector<listener_record> listeners;
        int32_t error_code = listeners.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = listeners.push_back(ft_move(entry));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        this->_listener_catalog.insert(listeners[0].metadata.hook_identifier, listeners);
        return (this->_listener_catalog.get_error());
    }
    auto &listeners = listeners_pair->value;
    const ft_size_t count = listeners.size();
    const listener_record *items = listeners.begin();
    ft_size_t insert_index = count;
    ft_size_t previous_index = count;
    ft_size_t index = 0U;
    while (index < count)
    {
        if (insert_index == count && entry.priority > items[index].priority)
            insert_index = index;
        if (items[index].metadata.listener_name == entry.metadata.listener_name)
            previous_index = index;
        if (insert_index != count && previous_index != count)
            break ;
        ++index;
    }
    const int32_t error_code = listeners.push_back(ft_move(entry));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    listener_record *updated = listeners.begin();
    if (insert_index < count)
    {
        // Registry entries own initialized strings without individual mutexes.
        // Move them after the fallible append so reordering needs no allocations.
        listener_record appended;
        (void)appended.initialize(ft_move(updated[count]));
        ft_size_t last_index = count;
        while (last_index > insert_index)
        {
            (void)updated[last_index].initialize(ft_move(updated[last_index - 1U]));
            --last_index;
        }
        (void)updated[insert_index].initialize(ft_move(appended));
    }
    if (previous_index < count)
    {
        if (previous_index >= insert_index)
            ++previous_index;
        listeners.erase(updated + previous_index);
        replaced = FT_TRUE;
    }
    return (FT_ERR_SUCCESS);
}

void game_hooks::insert_listener_unlocked(listener_record &entry) noexcept
{
    const ft_size_t old_count = this->_catalog_metadata.size();
    int32_t result = this->_catalog_metadata.push_back(entry.metadata);
    if (result == FT_ERR_SUCCESS)
    {
        ft_bool replaced = FT_FALSE;
        result = this->insert_callback_unlocked(entry, replaced);
        if (result != FT_ERR_SUCCESS)
            this->_catalog_metadata.pop_back();
        else if (replaced)
        {
            const ft_game_hook_metadata &new_metadata = this->_catalog_metadata[old_count];
            const ft_size_t previous_index = this->find_metadata_index_unlocked(
                new_metadata.hook_identifier, new_metadata.listener_name);
            if (previous_index < old_count)
                this->_catalog_metadata.erase(this->_catalog_metadata.begin() + previous_index);
        }
    }
    this->set_error(result);
    return ;
}

template <typename Callback>
void game_hooks::set_legacy_callback(Callback &stored_callback, Callback &&callback,
    hook_callback (*make_adapter)(Callback &&) noexcept, const char *hook_identifier,
    const char *listener_name, const char *description, const char *argument_contract) noexcept
{
    Callback staged(ft_move(callback));
    Callback copy(staged);
    hook_callback adapter;
    if (staged)
    {
        if (copy)
            adapter = make_adapter(ft_move(copy));
        if (!adapter)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            return ;
        }
    }
    listener_record entry;
    int32_t result = game_hooks_initialize_metadata(entry.metadata, hook_identifier,
        listener_name, description, argument_contract);
    if (result == FT_ERR_SUCCESS)
        result = entry.set_callback(ft_move(adapter));
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return ;
    }
    entry.priority = 1000;
    ft_bool lock_acquired;
    result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return ;
    }
    auto *bucket = this->_listener_catalog.find_as(hook_identifier);
    if (bucket != this->_listener_catalog.end())
    {
        listener_record *listeners = bucket->value.begin();
        const ft_size_t count = bucket->value.size();
        ft_size_t index = 0U;
        while (index < count)
        {
            if (listeners[index].metadata.listener_name == listener_name)
            {
                if (listeners[index].priority == entry.priority)
                {
                    result = listeners[index].callback.move(entry.callback);
                    if (result == FT_ERR_SUCCESS)
                        stored_callback = ft_move(staged);
                    (void)this->unlock_internal(lock_acquired);
                    this->set_error(result);
                    return ;
                }
                break ;
            }
            ++index;
        }
    }
    this->insert_listener_unlocked(entry);
    if (this->get_error() == FT_ERR_SUCCESS)
        stored_callback = ft_move(staged);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename Callback>
Callback game_hooks::get_legacy_callback(const Callback &stored_callback) const noexcept
{
    ft_bool lock_acquired;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (Callback());
    }
    Callback copy(stored_callback);
    int32_t result = FT_ERR_SUCCESS;
    if (stored_callback && !copy)
        result = FT_ERR_NO_MEMORY;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return (copy);
}

void game_hooks::set_on_item_crafted(
    ft_function<void(game_character&, game_item&)> &&callback) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::set_on_item_crafted");
    this->set_legacy_callback(this->_legacy_item_crafted, ft_move(callback),
        ft_game_hook_make_character_item_adapter, ft_game_hook_item_crafted_identifier,
        "legacy.item_crafted", "Legacy callback set via set_on_item_crafted",
        "game_character&,game_item&");
    return ;
}

void game_hooks::set_on_character_damaged(
    ft_function<void(game_character&, int32_t, uint8_t)> &&callback) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::set_on_character_damaged");
    this->set_legacy_callback(this->_legacy_character_damaged, ft_move(callback),
        ft_game_hook_make_character_damage_adapter, ft_game_hook_character_damaged_identifier,
        "legacy.character_damaged", "Legacy callback set via set_on_character_damaged",
        "game_character&,int32_t,uint8_t");
    return ;
}

void game_hooks::set_on_event_triggered(
    ft_function<void(game_world&, game_event&)> &&callback) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::set_on_event_triggered");
    this->set_legacy_callback(this->_legacy_event_triggered, ft_move(callback),
        ft_game_hook_make_world_event_adapter, ft_game_hook_event_triggered_identifier,
        "legacy.event_triggered", "Legacy callback set via set_on_event_triggered",
        "game_world&,game_event&");
    return ;
}

ft_function<void(game_character&, game_item&)> game_hooks::get_on_item_crafted() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::get_on_item_crafted");
    return (this->get_legacy_callback(this->_legacy_item_crafted));
}

ft_function<void(game_character&, int32_t, uint8_t)> game_hooks::get_on_character_damaged() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::get_on_character_damaged");
    return (this->get_legacy_callback(this->_legacy_character_damaged));
}

ft_function<void(game_world&, game_event&)> game_hooks::get_on_event_triggered() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::get_on_event_triggered");
    return (this->get_legacy_callback(this->_legacy_event_triggered));
}


template <typename Identifier>
void game_hooks::invoke_hook_by_key(const Identifier &hook_identifier,
    ft_game_hook_context &context) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::invoke_hook");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return ;
    }
    const auto *listeners_pair = this->_listener_catalog.find_as(hook_identifier);
    if (listeners_pair != this->_listener_catalog.end())
    {
        this->invoke_listeners(listeners_pair->value, lock_acquired, context);
        return ;
    }
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void game_hooks::invoke_listeners(const ft_vector<listener_record> &listeners,
    ft_bool lock_acquired, ft_game_hook_context &context) const noexcept
{
    const ft_size_t count = listeners.size();
    if (count <= 1U)
    {
        callback_owner retained;
        int32_t result = FT_ERR_SUCCESS;
        if (count == 1U)
            result = retained.initialize(listeners.begin()->callback);
        (void)this->unlock_internal(lock_acquired);
        if (result != FT_ERR_SUCCESS)
            this->set_error(result);
        else if (count == 1U)
        {
            const hook_callback *callback = retained.get();
            if (callback != ft_nullptr)
                (*callback)(context);
        }
        return ;
    }
    ft_vector<callback_owner> retained(count);
    int32_t result = retained.initialize();
    const listener_record *entries = listeners.begin();
    ft_size_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < count)
    {
        result = retained.push_back(entries[index].callback);
        ++index;
    }
    (void)this->unlock_internal(lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return ;
    }
    const callback_owner *callbacks = retained.begin();
    index = 0U;
    while (index < count)
    {
        const hook_callback *callback = callbacks[index].get();
        if (callback != ft_nullptr)
            (*callback)(context);
        ++index;
    }
    return ;
}

void game_hooks::invoke_hook(const ft_string &hook_identifier,
    ft_game_hook_context &context) const noexcept
{
    this->invoke_hook_by_key(hook_identifier, context);
    return ;
}

void game_hooks::invoke_on_item_crafted(game_character &character,
    game_item &item) const noexcept
{
    ft_game_hook_context context;

    context.world = ft_nullptr;
    context.character = &character;
    context.item = &item;
    context.event = ft_nullptr;
    context.integer_payload = 0;
    context.small_payload = 0;
    this->invoke_hook_by_key(ft_game_hook_item_crafted_identifier, context);
    return ;
}

void game_hooks::invoke_on_character_damaged(game_character &character,
    int32_t damage, uint8_t type) const noexcept
{
    ft_game_hook_context context;

    context.world = ft_nullptr;
    context.character = &character;
    context.item = ft_nullptr;
    context.event = ft_nullptr;
    context.integer_payload = damage;
    context.small_payload = type;
    this->invoke_hook_by_key(ft_game_hook_character_damaged_identifier, context);
    return ;
}

void game_hooks::invoke_on_event_triggered(game_world &world,
    game_event &event) const noexcept
{
    ft_game_hook_context context;

    context.world = &world;
    context.character = ft_nullptr;
    context.item = ft_nullptr;
    context.event = &event;
    context.integer_payload = 0;
    context.small_payload = 0;
    this->invoke_hook_by_key(ft_game_hook_event_triggered_identifier, context);
    return ;
}

void game_hooks::register_listener(const ft_game_hook_metadata &metadata,
    int32_t priority, ft_function<void(ft_game_hook_context&)> &&callback) noexcept
{
    listener_record entry;
    ft_bool lock_acquired;
    int32_t metadata_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::register_listener");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    metadata_error = entry.metadata.initialize(metadata);
    if (metadata_error != FT_ERR_SUCCESS)
    {
        this->set_error(metadata_error);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    entry.priority = priority;
    metadata_error = entry.set_callback(ft_move(callback));
    if (metadata_error != FT_ERR_SUCCESS)
    {
        this->set_error(metadata_error);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->insert_listener_unlocked(entry);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void game_hooks::unregister_listener(const ft_string &hook_identifier,
    const ft_string &listener_name) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::unregister_listener");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->remove_listener_unlocked(hook_identifier, listener_name);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

static void game_hooks_delete_metadata_vector(
    ft_vector<ft_game_hook_metadata> *metadata) noexcept
{
    if (metadata == ft_nullptr)
        return ;
    (void)metadata->destroy();
    delete metadata;
    return ;
}

ft_vector<ft_game_hook_metadata> *game_hooks::get_catalog_metadata() const noexcept
{
    ft_vector<ft_game_hook_metadata> *result;
    ft_bool lock_acquired;
    int32_t result_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::get_catalog_metadata");
    result = new (std::nothrow) ft_vector<ft_game_hook_metadata>();
    if (result == ft_nullptr)
    {
        const_cast<game_hooks *>(this)->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (result->initialize() != FT_ERR_SUCCESS)
    {
        const_cast<game_hooks *>(this)->set_error(result->get_error());
        delete result;
        return (ft_nullptr);
    }
    result_error = this->lock_internal(&lock_acquired);
    if (result_error == FT_ERR_SUCCESS)
    {
        result_error = result->replace_from(this->_catalog_metadata);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result_error);
    if (result_error != FT_ERR_SUCCESS)
    {
        game_hooks_delete_metadata_vector(result);
        return (ft_nullptr);
    }
    return (result);
}

ft_vector<ft_game_hook_metadata> *game_hooks::get_catalog_metadata_for(
    const ft_string &hook_identifier) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::get_catalog_metadata_for");
    auto *result = new (std::nothrow) ft_vector<ft_game_hook_metadata>();
    if (result == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    int32_t copy_error = result->initialize();
    if (copy_error != FT_ERR_SUCCESS)
    {
        this->set_error(copy_error);
        delete result;
        return (ft_nullptr);
    }
    ft_bool lock_acquired;
    copy_error = this->lock_internal(&lock_acquired);
    if (copy_error == FT_ERR_SUCCESS)
    {
        const auto *bucket = this->_listener_catalog.find(hook_identifier);
        if (bucket != this->_listener_catalog.end())
        {
            const ft_size_t matching_count = bucket->value.size();
            const ft_size_t total_count = this->_catalog_metadata.size();
            if (matching_count != 0U && matching_count == total_count)
                copy_error = result->replace_from(this->_catalog_metadata);
            else if (matching_count != 0U)
            {
                result->reserve(matching_count);
                copy_error = result->get_error();
                const ft_game_hook_metadata *items = this->_catalog_metadata.begin();
                ft_size_t index = 0U;
                ft_size_t copied_count = 0U;
                while (copy_error == FT_ERR_SUCCESS && copied_count < matching_count
                    && index < total_count)
                {
                    if (items[index].hook_identifier == hook_identifier)
                    {
                        copy_error = result->push_back(items[index]);
                        ++copied_count;
                    }
                    ++index;
                }
            }
        }
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(copy_error);
    if (copy_error != FT_ERR_SUCCESS)
    {
        game_hooks_delete_metadata_vector(result);
        return (ft_nullptr);
    }
    return (result);
}

void game_hooks::reset() noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_hooks::reset");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_legacy_item_crafted = ft_function<void(game_character&, game_item&)>();
    this->_legacy_character_damaged = ft_function<void(game_character&, int32_t, uint8_t)>();
    this->_legacy_event_triggered = ft_function<void(game_world&, game_event&)>();
    this->_listener_catalog.clear();
    this->_catalog_metadata.clear();
    (void)this->unlock_internal(lock_acquired);
    return ;
}
