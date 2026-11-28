#include "../PThread/pthread_internal.hpp"
#include "game_data_catalog.hpp"
#include "../Template/move.hpp"
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

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
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
    return (this->set_error(FT_ERR_SUCCESS));
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
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_item_definition::initialize(const game_item_definition &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_item_definition::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = game_item_definition::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t result = this->destroy();
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        item_values values;
        const int32_t result = other.read_values(values);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        this->write_values(values);
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::initialize(game_item_definition &&other) noexcept
{
    return (this->move(other));
}

int32_t game_item_definition::move(game_item_definition &other) noexcept
{
    if (this == &other && other._initialised_state != FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t result = this->initialize(static_cast<const game_item_definition &>(other));
    if (result != FT_ERR_SUCCESS || other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (result);
    const int32_t source_error = game_item_definition::_last_error;
    const int32_t destroy_error = other.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(destroy_error));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_definition::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t result = this->disable_thread_safety();
    this->_item_id = 0;
    this->_rarity = 0;
    this->_max_stack = 0;
    this->_width = 0;
    this->_height = 0;
    this->_weight = 0;
    this->_slot_requirement = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
}

int32_t game_item_definition::read_values(item_values &values) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    values = {this->_item_id, this->_rarity, this->_max_stack, this->_width,
        this->_height, this->_weight, this->_slot_requirement};
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_item_definition::write_values(const item_values &values) noexcept
{
    this->_item_id = values.item_id;
    this->_rarity = values.rarity;
    this->_max_stack = values.max_stack;
    this->_width = values.width;
    this->_height = values.height;
    this->_weight = values.weight;
    this->_slot_requirement = values.slot_requirement;
    return ;
}

int32_t game_item_definition::copy_values(const game_item_definition &other) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->initialize(other));
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    item_values values;
    int32_t result = other.read_values(values);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool lock_acquired = FT_FALSE;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->write_values(values);
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
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

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
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
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_recipe_blueprint::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t result = this->_ingredients.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->disable_thread_safety();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_recipe_id = 0;
    this->_result_item_id = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_recipe_blueprint::initialize(int32_t recipe_id, int32_t result_item_id,
    const ft_vector<game_crafting_ingredient> &ingredients) noexcept
{
    int32_t result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_ingredients.replace_from(ingredients);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->_recipe_id = recipe_id;
    this->_result_item_id = result_item_id;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_recipe_blueprint::initialize(const game_recipe_blueprint &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_recipe_blueprint::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_ingredients.replace_from(other._ingredients);
        if (result == FT_ERR_SUCCESS)
        {
            this->_recipe_id = other._recipe_id;
            this->_result_item_id = other._result_item_id;
        }
        other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::initialize(game_recipe_blueprint &&other) noexcept
{
    return (this->move(other));
}

int32_t game_recipe_blueprint::move(game_recipe_blueprint &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_recipe_blueprint::move",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_ingredients.move(other._ingredients);
        if (result == FT_ERR_SUCCESS)
        {
            this->_recipe_id = other._recipe_id;
            this->_result_item_id = other._result_item_id;
        }
        other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_recipe_blueprint::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    const int32_t vector_error = this->_ingredients.destroy();
    this->_recipe_id = 0;
    this->_result_item_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(vector_error));
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
    const int32_t result = this->_ingredients.replace_from(ingredients);
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
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

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
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
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_entry::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = game_loadout_entry::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t result = this->destroy();
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        ft_bool source_locked = FT_FALSE;
        const int32_t result = other.lock_internal(&source_locked);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        this->_slot = other._slot;
        this->_item_id = other._item_id;
        this->_quantity = other._quantity;
        other.unlock_internal(source_locked);
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::initialize(game_loadout_entry &&other) noexcept
{
    return (this->move(other));
}

int32_t game_loadout_entry::move(game_loadout_entry &other) noexcept
{
    if (this == &other && other._initialised_state != FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t result = this->initialize(static_cast<const game_loadout_entry &>(other));
    if (result != FT_ERR_SUCCESS || other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (result);
    const int32_t source_error = game_loadout_entry::_last_error;
    const int32_t destroy_error = other.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(destroy_error));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_entry::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t result = this->disable_thread_safety();
    this->_slot = 0;
    this->_item_id = 0;
    this->_quantity = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
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

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
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
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_loadout_blueprint::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t result = this->_entries.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->disable_thread_safety();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_loadout_id = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_loadout_blueprint::initialize(int32_t loadout_id,
    const ft_vector<game_loadout_entry> &entries) noexcept
{
    int32_t result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_entries.replace_from(entries);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->_loadout_id = loadout_id;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_loadout_blueprint::initialize(const game_loadout_blueprint &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_blueprint::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_entries.replace_from(other._entries);
        if (result == FT_ERR_SUCCESS)
        {
            this->_loadout_id = other._loadout_id;
        }
        other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::initialize(game_loadout_blueprint &&other) noexcept
{
    return (this->move(other));
}

int32_t game_loadout_blueprint::move(game_loadout_blueprint &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_loadout_blueprint::move",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_entries.move(other._entries);
        if (result == FT_ERR_SUCCESS)
        {
            this->_loadout_id = other._loadout_id;
        }
        other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_loadout_blueprint::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    const int32_t vector_error = this->_entries.destroy();
    this->_loadout_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(vector_error));
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
    const int32_t result = this->_entries.replace_from(entries);
    this->unlock_internal(lock_acquired);
    this->set_error(result);
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

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
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
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_data_catalog::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t result = this->_item_definitions.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_recipes.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_loadouts.initialize();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(result));
}

int32_t game_data_catalog::initialize(const game_data_catalog &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_data_catalog::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_item_definitions.copy_from(other._item_definitions);
        if (result == FT_ERR_SUCCESS)
            result = this->_recipes.copy_from(other._recipes);
        if (result == FT_ERR_SUCCESS)
            result = this->_loadouts.copy_from(other._loadouts);
        other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::initialize(game_data_catalog &&other) noexcept
{
    return (this->move(other));
}

int32_t game_data_catalog::move(game_data_catalog &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_data_catalog::move",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    ft_bool items_locked = FT_FALSE;
    ft_bool recipes_locked = FT_FALSE;
    ft_bool loadouts_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._item_definitions.lock(&items_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._recipes.lock(&recipes_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._loadouts.lock(&loadouts_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_item_definitions.move(other._item_definitions);
        const ft_bool items_moved = result == FT_ERR_SUCCESS;
        if (items_moved)
            result = this->_recipes.move(other._recipes);
        const ft_bool recipes_moved = result == FT_ERR_SUCCESS;
        if (recipes_moved)
            result = this->_loadouts.move(other._loadouts);
        if (result != FT_ERR_SUCCESS)
        {
            // Roll back any earlier transfers while the source maps are locked.
            if (recipes_moved)
                (void)other._recipes.move(this->_recipes);
            if (items_moved)
                (void)other._item_definitions.move(this->_item_definitions);
        }
    }
    if (loadouts_locked)
        (void)other._loadouts.unlock(loadouts_locked);
    if (recipes_locked)
        (void)other._recipes.unlock(recipes_locked);
    if (items_locked)
        (void)other._item_definitions.unlock(items_locked);
    other.unlock_internal(source_locked);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_data_catalog::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    const int32_t item_error = this->_item_definitions.destroy();
    if (result == FT_ERR_SUCCESS)
        result = item_error;
    const int32_t recipe_error = this->_recipes.destroy();
    if (result == FT_ERR_SUCCESS)
        result = recipe_error;
    const int32_t loadout_error = this->_loadouts.destroy();
    if (result == FT_ERR_SUCCESS)
        result = loadout_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
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
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::register_item_definition");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        errno_abort_if_uninitialised_or_destroyed(definition._initialised_state,
            "game_data_catalog::register_item_definition source");
        game_item_definition snapshot;
        result = snapshot.initialize(definition);
        if (result == FT_ERR_SUCCESS)
        {
            this->_item_definitions.insert(snapshot._item_id, snapshot);
            result = this->_item_definitions.get_error();
        }
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_data_catalog::register_recipe(const game_recipe_blueprint &recipe) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::register_recipe");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        game_recipe_blueprint snapshot;
        result = snapshot.initialize(recipe);
        if (result == FT_ERR_SUCCESS)
        {
            const int32_t identifier = snapshot.get_recipe_id();
            this->_recipes.insert(identifier, ft_move(snapshot));
            result = this->_recipes.get_error();
        }
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_data_catalog::register_loadout(const game_loadout_blueprint &loadout) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::register_loadout");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        game_loadout_blueprint snapshot;
        result = snapshot.initialize(loadout);
        if (result == FT_ERR_SUCCESS)
        {
            const int32_t identifier = snapshot.get_loadout_id();
            this->_loadouts.insert(identifier, ft_move(snapshot));
            result = this->_loadouts.get_error();
        }
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_data_catalog::fetch_item_definition(int32_t item_id,
    game_item_definition &definition) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::fetch_item_definition");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_item_definitions.visit_value(item_id,
            [&definition](const game_item_definition &stored) noexcept
            {
                return (definition.copy_values(stored));
            });
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_data_catalog::fetch_recipe(int32_t recipe_id,
    game_recipe_blueprint &recipe) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::fetch_recipe");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_recipes.visit_value(recipe_id,
            [&recipe](const game_recipe_blueprint &stored) noexcept
            {
                return (recipe.initialize(stored));
            });
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_data_catalog::fetch_loadout(int32_t loadout_id,
    game_loadout_blueprint &loadout) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_data_catalog::fetch_loadout");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_loadouts.visit_value(loadout_id,
            [&loadout](const game_loadout_blueprint &stored) noexcept
            {
                return (loadout.initialize(stored));
            });
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}
