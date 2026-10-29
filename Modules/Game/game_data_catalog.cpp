#include "../PThread/pthread_internal.hpp"
#include "game_data_catalog.hpp"
#include "../Template/move.hpp"
#include "../PThread/pthread.hpp"
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
#include "../Template/vector.hpp"
#include "game_crafting.hpp"

thread_local int32_t game_item_definition::_last_error = FT_ERR_SUCCESS;

int32_t game_item_definition::set_error(int32_t error_code) noexcept
{
    game_item_definition::_last_error = error_code;
    return (error_code);
}

int32_t game_item_definition::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item_definition::get_error");
    return (game_item_definition::_last_error);
}

const char *game_item_definition::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item_definition::get_error_str");
    return (ft_strerror(this->get_error()));
}

thread_local int32_t game_loadout_entry::_last_error = FT_ERR_SUCCESS;

int32_t game_loadout_entry::set_error(int32_t error_code) noexcept
{
    game_loadout_entry::_last_error = error_code;
    return (error_code);
}

int32_t game_loadout_entry::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_loadout_entry::get_error");
    return (game_loadout_entry::_last_error);
}

const char *game_loadout_entry::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_loadout_entry::get_error_str");
    return (ft_strerror(this->get_error()));
}

thread_local int32_t game_loadout_blueprint::_last_error = FT_ERR_SUCCESS;

int32_t game_loadout_blueprint::set_error(int32_t error_code) noexcept
{
    game_loadout_blueprint::_last_error = error_code;
    return (error_code);
}

int32_t game_loadout_blueprint::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_loadout_blueprint::get_error");
    return (game_loadout_blueprint::_last_error);
}

const char *game_loadout_blueprint::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_loadout_blueprint::get_error_str");
    return (ft_strerror(this->get_error()));
}

thread_local int32_t game_recipe_blueprint::_last_error = FT_ERR_SUCCESS;

int32_t game_recipe_blueprint::set_error(int32_t error_code) noexcept
{
    game_recipe_blueprint::_last_error = error_code;
    return (error_code);
}

int32_t game_recipe_blueprint::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_recipe_blueprint::get_error");
    return (game_recipe_blueprint::_last_error);
}

const char *game_recipe_blueprint::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_recipe_blueprint::get_error_str");
    return (ft_strerror(this->get_error()));
}

static void game_data_catalog_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

static void game_data_catalog_copy_crafting_vector(const ft_vector<game_crafting_ingredient> &source,
        ft_vector<game_crafting_ingredient> &destination)
{
    ft_vector<game_crafting_ingredient>::const_iterator entry;
    ft_vector<game_crafting_ingredient>::const_iterator end;

    destination.clear();
    entry = source.begin();
    end = source.end();
    while (entry != end)
    {
        destination.push_back(*entry);
        ++entry;
    }
    return ;
}

static void game_data_catalog_copy_loadout_vector(const ft_vector<game_loadout_entry> &source,
        ft_vector<game_loadout_entry> &destination)
{
    ft_vector<game_loadout_entry>::const_iterator entry;
    ft_vector<game_loadout_entry>::const_iterator end;

    destination.clear();
    entry = source.begin();
    end = source.end();
    while (entry != end)
    {
        destination.push_back(*entry);
        ++entry;
    }
    return ;
}

int32_t game_item_definition::lock_pair(const game_item_definition &first,
        const game_item_definition &second,
        ft_bool *first_locked,
        ft_bool *second_locked)
{
    const game_item_definition *ordered_first;
    const game_item_definition *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_item_definition *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_data_catalog_sleep_backoff();
    }
}

game_item_definition::game_item_definition() noexcept
    : _item_id(0), _rarity(0), _max_stack(0), _width(0), _height(0), _weight(0),
      _slot_requirement(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_item_definition::~game_item_definition() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    return ;
}

int32_t game_item_definition::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_item_definition::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_item_definition::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::lock_internal");
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

void game_item_definition::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_item_definition::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_item_definition::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_item_id = 0;
    this->_rarity = 0;
    this->_max_stack = 0;
    this->_width = 0;
    this->_height = 0;
    this->_weight = 0;
    this->_slot_requirement = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::initialize(int32_t item_id, int32_t rarity, int32_t max_stack,
    int32_t width, int32_t height, int32_t weight, int32_t slot_requirement) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_item_definition::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_item_id = item_id;
    this->_rarity = rarity;
    this->_max_stack = max_stack;
    this->_width = width;
    this->_height = height;
    this->_weight = weight;
    this->_slot_requirement = slot_requirement;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::initialize(const game_item_definition &other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_item_definition::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_item_id = other._item_id;
    this->_rarity = other._rarity;
    this->_max_stack = other._max_stack;
    this->_width = other._width;
    this->_height = other._height;
    this->_weight = other._weight;
    this->_slot_requirement = other._slot_requirement;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::initialize(game_item_definition &&other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_item_definition::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_item_id = other._item_id;
    this->_rarity = other._rarity;
    this->_max_stack = other._max_stack;
    this->_width = other._width;
    this->_height = other._height;
    this->_weight = other._weight;
    this->_slot_requirement = other._slot_requirement;
    other._item_id = 0;
    other._rarity = 0;
    other._max_stack = 0;
    other._width = 0;
    other._height = 0;
    other._weight = 0;
    other._slot_requirement = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::move(game_item_definition &other) noexcept
{
    return (this->initialize(static_cast<game_item_definition &&>(other)));
}

int32_t game_item_definition::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_item_id = 0;
    this->_rarity = 0;
    this->_max_stack = 0;
    this->_width = 0;
    this->_height = 0;
    this->_weight = 0;
    this->_slot_requirement = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::get_item_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = this->_item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (identifier);
}

void game_item_definition::set_item_id(int32_t item_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_item_id = item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item_definition::get_rarity() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t rarity_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_rarity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    rarity_value = this->_rarity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (rarity_value);
}

void game_item_definition::set_rarity(int32_t rarity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_rarity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_rarity = rarity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item_definition::get_max_stack() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_max_stack");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    value = this->_max_stack;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (value);
}

void game_item_definition::set_max_stack(int32_t max_stack) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_max_stack");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_max_stack = max_stack;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item_definition::get_width() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t width_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_width");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    width_value = this->_width;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (width_value);
}

void game_item_definition::set_width(int32_t width) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_width");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_width = width;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}


int32_t game_item_definition::get_height() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t height_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_height");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    height_value = this->_height;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (height_value);
}

void game_item_definition::set_height(int32_t height) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_height");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_height = height;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item_definition::get_weight() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t weight_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_weight");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    weight_value = this->_weight;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (weight_value);
}

void game_item_definition::set_weight(int32_t weight) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_weight");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_weight = weight;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item_definition::get_slot_requirement() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t slot_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::get_slot_requirement");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    slot_value = this->_slot_requirement;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (slot_value);
}

void game_item_definition::set_slot_requirement(int32_t slot_requirement) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_item_definition::set_slot_requirement");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_slot_requirement = slot_requirement;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}



int32_t game_recipe_blueprint::lock_pair(const game_recipe_blueprint &first,
        const game_recipe_blueprint &second,
        ft_bool *first_locked,
        ft_bool *second_locked)
{
    const game_recipe_blueprint *ordered_first;
    const game_recipe_blueprint *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_recipe_blueprint *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_data_catalog_sleep_backoff();
    }
}

game_recipe_blueprint::game_recipe_blueprint() noexcept
    : _recipe_id(0), _result_item_id(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_recipe_blueprint::~game_recipe_blueprint() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    return ;
}

int32_t game_recipe_blueprint::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_recipe_blueprint::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_recipe_blueprint::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::lock_internal");
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

void game_recipe_blueprint::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_recipe_blueprint::initialize() noexcept
{
    int32_t vector_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_recipe_blueprint::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    vector_initialize_error = this->_ingredients.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    this->_recipe_id = 0;
    this->_result_item_id = 0;
    this->_ingredients.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::initialize(int32_t recipe_id, int32_t result_item_id,
    const ft_vector<game_crafting_ingredient> &ingredients) noexcept
{
    int32_t vector_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_recipe_blueprint::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    vector_initialize_error = this->_ingredients.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    this->_recipe_id = recipe_id;
    this->_result_item_id = result_item_id;
    game_data_catalog_copy_crafting_vector(ingredients, this->_ingredients);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::initialize(const game_recipe_blueprint &other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    int32_t vector_initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_recipe_blueprint::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    vector_initialize_error = this->_ingredients.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_ingredients.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_recipe_id = other._recipe_id;
    this->_result_item_id = other._result_item_id;
    game_data_catalog_copy_crafting_vector(other._ingredients, this->_ingredients);
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::initialize(game_recipe_blueprint &&other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    int32_t vector_initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_recipe_blueprint::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    vector_initialize_error = this->_ingredients.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_ingredients.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_recipe_id = other._recipe_id;
    this->_result_item_id = other._result_item_id;
    game_data_catalog_copy_crafting_vector(other._ingredients, this->_ingredients);
    other._recipe_id = 0;
    other._result_item_id = 0;
    other._ingredients.clear();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::move(game_recipe_blueprint &other) noexcept
{
    return (this->initialize(static_cast<game_recipe_blueprint &&>(other)));
}

int32_t game_recipe_blueprint::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)this->_ingredients.destroy();
    this->_recipe_id = 0;
    this->_result_item_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}


int32_t game_recipe_blueprint::get_recipe_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::get_recipe_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = this->_recipe_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (identifier);
}

void game_recipe_blueprint::set_recipe_id(int32_t recipe_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::set_recipe_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_recipe_id = recipe_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_recipe_blueprint::get_result_item_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t item_id;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::get_result_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    item_id = this->_result_item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item_id);
}

void game_recipe_blueprint::set_result_item_id(int32_t result_item_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::set_result_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_result_item_id = result_item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_vector<game_crafting_ingredient> &game_recipe_blueprint::get_ingredients() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::get_ingredients");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_ingredients);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_ingredients);
}

const ft_vector<game_crafting_ingredient> &game_recipe_blueprint::get_ingredients() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::get_ingredients const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_ingredients);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_ingredients);
}

void game_recipe_blueprint::set_ingredients(const ft_vector<game_crafting_ingredient> &ingredients) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_recipe_blueprint::set_ingredients");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    game_data_catalog_copy_crafting_vector(ingredients, this->_ingredients);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}


int32_t game_loadout_entry::lock_pair(const game_loadout_entry &first,
        const game_loadout_entry &second,
        ft_bool *first_locked,
        ft_bool *second_locked)
{
    const game_loadout_entry *ordered_first;
    const game_loadout_entry *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_loadout_entry *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_data_catalog_sleep_backoff();
    }
}

game_loadout_entry::game_loadout_entry() noexcept
    : _slot(0), _item_id(0), _quantity(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_loadout_entry::~game_loadout_entry() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    return ;
}

int32_t game_loadout_entry::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_loadout_entry::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_loadout_entry::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::lock_internal");
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

void game_loadout_entry::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_loadout_entry::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_loadout_entry::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_slot = 0;
    this->_item_id = 0;
    this->_quantity = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::initialize(int32_t slot, int32_t item_id, int32_t quantity) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_loadout_entry::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_slot = slot;
    this->_item_id = item_id;
    this->_quantity = quantity;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::initialize(const game_loadout_entry &other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_entry::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_slot = other._slot;
    this->_item_id = other._item_id;
    this->_quantity = other._quantity;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::initialize(game_loadout_entry &&other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_entry::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_slot = other._slot;
    this->_item_id = other._item_id;
    this->_quantity = other._quantity;
    other._slot = 0;
    other._item_id = 0;
    other._quantity = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::move(game_loadout_entry &other) noexcept
{
    return (this->initialize(static_cast<game_loadout_entry &&>(other)));
}

int32_t game_loadout_entry::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_slot = 0;
    this->_item_id = 0;
    this->_quantity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}


int32_t game_loadout_entry::get_slot() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t slot_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::get_slot");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    slot_value = this->_slot;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (slot_value);
}

void game_loadout_entry::set_slot(int32_t slot) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::set_slot");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_slot = slot;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_loadout_entry::get_item_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t item_id;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::get_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    item_id = this->_item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item_id);
}

void game_loadout_entry::set_item_id(int32_t item_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::set_item_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_item_id = item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_loadout_entry::get_quantity() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t quantity_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::get_quantity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    quantity_value = this->_quantity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (quantity_value);
}

void game_loadout_entry::set_quantity(int32_t quantity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_entry::set_quantity");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_quantity = quantity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}


int32_t game_loadout_blueprint::lock_pair(const game_loadout_blueprint &first,
        const game_loadout_blueprint &second,
        ft_bool *first_locked,
        ft_bool *second_locked)
{
    const game_loadout_blueprint *ordered_first;
    const game_loadout_blueprint *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_loadout_blueprint *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_data_catalog_sleep_backoff();
    }
}

game_loadout_blueprint::game_loadout_blueprint() noexcept
    : _loadout_id(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_loadout_blueprint::~game_loadout_blueprint() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    return ;
}

int32_t game_loadout_blueprint::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_loadout_blueprint::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_loadout_blueprint::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::lock_internal");
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

void game_loadout_blueprint::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_loadout_blueprint::initialize() noexcept
{
    int32_t vector_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_loadout_blueprint::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    vector_initialize_error = this->_entries.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    this->_loadout_id = 0;
    this->_entries.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::initialize(int32_t loadout_id,
    const ft_vector<game_loadout_entry> &entries) noexcept
{
    int32_t vector_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_loadout_blueprint::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    vector_initialize_error = this->_entries.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    this->_loadout_id = loadout_id;
    game_data_catalog_copy_loadout_vector(entries, this->_entries);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::initialize(const game_loadout_blueprint &other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    int32_t vector_initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_blueprint::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    vector_initialize_error = this->_entries.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_entries.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_loadout_id = other._loadout_id;
    game_data_catalog_copy_loadout_vector(other._entries, this->_entries);
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::initialize(game_loadout_blueprint &&other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    int32_t vector_initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_blueprint::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    vector_initialize_error = this->_entries.initialize();
    if (vector_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (vector_initialize_error);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_entries.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_loadout_id = other._loadout_id;
    game_data_catalog_copy_loadout_vector(other._entries, this->_entries);
    other._loadout_id = 0;
    other._entries.clear();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::move(game_loadout_blueprint &other) noexcept
{
    return (this->initialize(static_cast<game_loadout_blueprint &&>(other)));
}

int32_t game_loadout_blueprint::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)this->_entries.destroy();
    this->_loadout_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}


int32_t game_loadout_blueprint::get_loadout_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::get_loadout_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = this->_loadout_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (identifier);
}

void game_loadout_blueprint::set_loadout_id(int32_t loadout_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::set_loadout_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_loadout_id = loadout_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_vector<game_loadout_entry> &game_loadout_blueprint::get_entries() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::get_entries");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_entries);
}

const ft_vector<game_loadout_entry> &game_loadout_blueprint::get_entries() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::get_entries const");
    const_cast<game_loadout_blueprint *>(this)->set_error(FT_ERR_SUCCESS);
    return (this->_entries);
}

void game_loadout_blueprint::set_entries(const ft_vector<game_loadout_entry> &entries) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_loadout_blueprint::set_entries");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    game_data_catalog_copy_loadout_vector(entries, this->_entries);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

thread_local int32_t game_data_catalog::_last_error = FT_ERR_SUCCESS;

int32_t game_data_catalog::set_error(int32_t error_code) noexcept
{
    game_data_catalog::_last_error = error_code;
    return (error_code);
}

int32_t game_data_catalog::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_data_catalog::get_error");
    return (game_data_catalog::_last_error);
}

const char *game_data_catalog::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_data_catalog::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_data_catalog::lock_pair(const game_data_catalog &first,
        const game_data_catalog &second,
        ft_bool *first_locked,
        ft_bool *second_locked)
{
    const game_data_catalog *ordered_first;
    const game_data_catalog *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_data_catalog *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_data_catalog_sleep_backoff();
    }
}


game_data_catalog::game_data_catalog() noexcept
    : _item_definitions(), _recipes(), _loadouts(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_data_catalog::~game_data_catalog() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    return ;
}

int32_t game_data_catalog::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_data_catalog::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_data_catalog::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::lock_internal");
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

void game_data_catalog::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_data_catalog::initialize() noexcept
{
    int32_t init_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_data_catalog::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    init_error = this->_item_definitions.initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (init_error);
    }
    init_error = this->_recipes.initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (init_error);
    }
    init_error = this->_loadouts.initialize();
    if (init_error != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        (void)this->_recipes.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (init_error);
    }
    this->_item_definitions.clear();
    this->_recipes.clear();
    this->_loadouts.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::initialize(const game_data_catalog &other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_data_catalog::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (this->_item_definitions.initialize() != FT_ERR_SUCCESS
        || this->_recipes.initialize() != FT_ERR_SUCCESS
        || this->_loadouts.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        (void)this->_recipes.destroy();
        (void)this->_loadouts.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_STATE);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        (void)this->_recipes.destroy();
        (void)this->_loadouts.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_item_definitions = other._item_definitions;
    this->_recipes = other._recipes;
    this->_loadouts = other._loadouts;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::initialize(game_data_catalog &&other) noexcept
{
    ft_bool other_locked;
    int32_t destroy_error;
    int32_t lock_error;
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_data_catalog::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (this->_item_definitions.initialize() != FT_ERR_SUCCESS
        || this->_recipes.initialize() != FT_ERR_SUCCESS
        || this->_loadouts.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        (void)this->_recipes.destroy();
        (void)this->_loadouts.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_STATE);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->_item_definitions.destroy();
        (void)this->_recipes.destroy();
        (void)this->_loadouts.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_item_definitions = other._item_definitions;
    this->_recipes = other._recipes;
    this->_loadouts = other._loadouts;
    other._item_definitions.clear();
    other._recipes.clear();
    other._loadouts.clear();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(other_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::move(game_data_catalog &other) noexcept
{
    return (this->initialize(static_cast<game_data_catalog &&>(other)));
}

int32_t game_data_catalog::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)this->_item_definitions.destroy();
    (void)this->_recipes.destroy();
    (void)this->_loadouts.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_map<int32_t, game_item_definition> &game_data_catalog::get_item_definitions() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_item_definitions");
    return (this->_item_definitions);
}

const ft_map<int32_t, game_item_definition> &game_data_catalog::get_item_definitions() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_item_definitions const");
    return (this->_item_definitions);
}

ft_map<int32_t, game_recipe_blueprint> &game_data_catalog::get_recipes() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_recipes");
    return (this->_recipes);
}

const ft_map<int32_t, game_recipe_blueprint> &game_data_catalog::get_recipes() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_recipes const");
    return (this->_recipes);
}

ft_map<int32_t, game_loadout_blueprint> &game_data_catalog::get_loadouts() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_loadouts");
    return (this->_loadouts);
}

const ft_map<int32_t, game_loadout_blueprint> &game_data_catalog::get_loadouts() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::get_loadouts const");
    return (this->_loadouts);
}

int32_t game_data_catalog::register_item_definition(const game_item_definition &definition) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;
    int32_t map_error;
    const Pair<int32_t, game_item_definition> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::register_item_definition");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = definition.get_item_id();
    this->_item_definitions.insert(identifier, definition);
    map_error = this->_item_definitions.get_error();
    if (map_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(map_error);
        return (map_error);
    }
    entry = this->_item_definitions.find(identifier);
    if (entry == this->_item_definitions.end())
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::register_recipe(const game_recipe_blueprint &recipe) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::register_recipe");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = recipe.get_recipe_id();
    this->_recipes.insert(identifier, recipe);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::register_loadout(const game_loadout_blueprint &loadout) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::register_loadout");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = loadout.get_loadout_id();
    this->_loadouts.insert(identifier, loadout);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::fetch_item_definition(int32_t item_id, game_item_definition &definition) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const game_data_catalog *self;
    const Pair<int32_t, game_item_definition> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::fetch_item_definition");
    self = this;
    lock_acquired = FT_FALSE;
    lock_error = self->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        self->set_error(lock_error);
        return (lock_error);
    }
    entry = self->_item_definitions.find(item_id);
    if (entry == self->_item_definitions.end())
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    if (definition.initialize(entry->value) != FT_ERR_SUCCESS)
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    self->set_error(FT_ERR_SUCCESS);
    self->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::fetch_recipe(int32_t recipe_id, game_recipe_blueprint &recipe) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const game_data_catalog *self;
    const Pair<int32_t, game_recipe_blueprint> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::fetch_recipe");
    self = this;
    lock_acquired = FT_FALSE;
    lock_error = self->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        self->set_error(lock_error);
        return (lock_error);
    }
    entry = self->_recipes.find(recipe_id);
    if (entry == self->_recipes.end())
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    if (recipe.initialize(entry->value) != FT_ERR_SUCCESS)
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    self->set_error(FT_ERR_SUCCESS);
    self->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::fetch_loadout(int32_t loadout_id, game_loadout_blueprint &loadout) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const game_data_catalog *self;
    const Pair<int32_t, game_loadout_blueprint> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_data_catalog::fetch_loadout");
    self = this;
    lock_acquired = FT_FALSE;
    lock_error = self->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        self->set_error(lock_error);
        return (lock_error);
    }
    entry = self->_loadouts.find(loadout_id);
    if (entry == self->_loadouts.end())
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    if (loadout.initialize(entry->value) != FT_ERR_SUCCESS)
    {
        self->unlock_internal(lock_acquired);
        self->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    self->set_error(FT_ERR_SUCCESS);
    self->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}
