#include "../Printf/printf.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "game_crafting.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"

thread_local int32_t game_crafting_ingredient::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_crafting::_last_error = FT_ERR_SUCCESS;

game_crafting_prepared::game_crafting_prepared() noexcept
    : _expected_items(), _result_items(), _expected_capacity(0U),
      _expected_used_slots(0U), _expected_weight_limit(0),
      _expected_current_weight(0), _expected_next_slot(0),
      _result_capacity(0U), _result_used_slots(0U), _result_weight_limit(0),
      _result_current_weight(0), _result_next_slot(0),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _ready(FT_FALSE)
{
}

game_crafting_prepared::~game_crafting_prepared() noexcept
{
    (void)this->destroy();
}

int32_t game_crafting_prepared::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    int32_t result = this->_expected_items.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_result_items.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_expected_items.destroy();
        return (result);
    }
    this->_ready = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_prepared::clear() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    this->_expected_items.clear();
    this->_result_items.clear();
    this->_ready = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_prepared::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->_expected_items.destroy();
    (void)this->_result_items.destroy();
    this->_ready = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ft_bool game_crafting_prepared::is_ready() const noexcept
{
    return (this->_ready);
}

int32_t game_crafting_ingredient::set_error(int32_t error_code) noexcept
{
    game_crafting_ingredient::_last_error = error_code;
    return (error_code);
}

int32_t game_crafting_ingredient::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_crafting_ingredient::get_error");
    return (game_crafting_ingredient::_last_error);
}

const char *game_crafting_ingredient::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_crafting_ingredient::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_crafting::set_error(int32_t error_code) noexcept
{
    game_crafting::_last_error = error_code;
    return (error_code);
}

int32_t game_crafting::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_crafting::get_error");
    return (game_crafting::_last_error);
}

const char *game_crafting::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_crafting::get_error_str");
    return (ft_strerror(game_crafting::_last_error));
}

game_crafting_ingredient::game_crafting_ingredient() noexcept
    : _item_id(0), _count(0), _rarity(-1),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    return ;
}

game_crafting_ingredient::game_crafting_ingredient(
    const game_crafting_ingredient &other) noexcept
    : _item_id(0), _count(0), _rarity(-1),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    (void)this->initialize(other);
    return ;
}

game_crafting_ingredient::game_crafting_ingredient(
    game_crafting_ingredient &&other) noexcept
    : _item_id(0), _count(0), _rarity(-1),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    (void)this->initialize(static_cast<game_crafting_ingredient &&>(other));
    return ;
}

game_crafting_ingredient::~game_crafting_ingredient() noexcept
{
    (void)this->destroy();
    return ;
}

game_crafting::game_crafting() noexcept
    : _recipes(), _unlocked_recipes(), _recipe_outputs(),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    return ;
}

game_crafting::~game_crafting() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_crafting::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "game_crafting::initialize", "initialize called on initialised instance");
    int32_t recipes_error = this->_recipes.initialize();
    if (recipes_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(recipes_error);
        return (recipes_error);
    }
    recipes_error = this->_unlocked_recipes.initialize();
    if (recipes_error != FT_ERR_SUCCESS)
    {
        (void)this->_recipes.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(recipes_error);
        return (recipes_error);
    }
    recipes_error = this->_recipe_outputs.initialize();
    if (recipes_error != FT_ERR_SUCCESS)
    {
        (void)this->_unlocked_recipes.destroy();
        (void)this->_recipes.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(recipes_error);
        return (recipes_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t recipes_error = this->_recipes.destroy();
    int32_t unlocks_error = this->_unlocked_recipes.destroy();
    int32_t outputs_error = this->_recipe_outputs.destroy();
    int32_t disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (recipes_error != FT_ERR_SUCCESS)
    {
        this->set_error(recipes_error);
        return (recipes_error);
    }
    if (unlocks_error != FT_ERR_SUCCESS)
    {
        this->set_error(unlocks_error);
        return (unlocks_error);
    }
    if (outputs_error != FT_ERR_SUCCESS)
    {
        this->set_error(outputs_error);
        return (outputs_error);
    }
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_crafting::move(game_crafting &other) noexcept
{
    int32_t destroy_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_crafting::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    this->_recipes = other._recipes;
    this->_unlocked_recipes = other._unlocked_recipes;
    this->_recipe_outputs = other._recipe_outputs;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._recipes.clear();
    other._unlocked_recipes.clear();
    other._recipe_outputs.clear();
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::initialize(
    const game_crafting_ingredient &other) noexcept
{
    ft_bool this_lock_acquired;
    ft_bool other_lock_acquired;
    int32_t lock_error;

    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "game_crafting_ingredient::initialize(copy)",
            "source object is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    this_lock_acquired = FT_FALSE;
    other_lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&this_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    lock_error = other.lock_internal(&other_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(this_lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_item_id = other._item_id;
    this->_count = other._count;
    this->_rarity = other._rarity;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other.unlock_internal(other_lock_acquired);
    this->unlock_internal(this_lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::initialize(game_crafting_ingredient &&other) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize(static_cast<const game_crafting_ingredient &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    other._item_id = 0;
    other._count = 0;
    other._rarity = -1;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::move(game_crafting_ingredient &other) noexcept
{
    int32_t destroy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (this->initialize(static_cast<game_crafting_ingredient &&>(other)));
}

int32_t game_crafting_ingredient::initialize(int32_t item_id, int32_t count, int32_t rarity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_item_id = item_id;
    this->_count = count;
    this->_rarity = rarity;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_item_id = 0;
    this->_count = 0;
    this->_rarity = -1;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_crafting_ingredient::lock_internal(ft_bool *lock_acquired) const noexcept
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

void game_crafting_ingredient::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_crafting_ingredient::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

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
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(mutex_error);
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_bool game_crafting_ingredient::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_crafting_ingredient::get_item_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t item_id;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    item_id = this->_item_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item_id);
}

void game_crafting_ingredient::set_item_id(int32_t item_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

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

int32_t game_crafting_ingredient::get_count() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t count;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    count = this->_count;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (count);
}

void game_crafting_ingredient::set_count(int32_t count) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (count < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_count = count;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_crafting_ingredient::get_rarity() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t rarity;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    rarity = this->_rarity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (rarity);
}

void game_crafting_ingredient::set_rarity(int32_t rarity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (rarity < -1)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
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

int32_t game_crafting::lock_internal(ft_bool *lock_acquired) const noexcept
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

void game_crafting::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_crafting::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::enable_thread_safety");
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
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(mutex_error);
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_bool game_crafting::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

ft_map<int32_t, ft_vector<game_crafting_ingredient>> &game_crafting::get_recipes() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::get_recipes");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_recipes);
}

const ft_map<int32_t, ft_vector<game_crafting_ingredient>> &game_crafting::get_recipes() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::get_recipes");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_recipes);
}

int32_t game_crafting::register_recipe(int32_t recipe_id,
    ft_vector<game_crafting_ingredient> &&ingredients) noexcept
{
    return (this->register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients), FT_TRUE));
}

int32_t game_crafting::register_recipe(int32_t recipe_id,
    ft_vector<game_crafting_ingredient> &&ingredients,
    ft_bool initially_unlocked) noexcept
{
    ft_vector<game_crafting_ingredient> copied_ingredients;
    ft_size_t ingredient_index;
    ft_size_t ingredient_count;
    ft_bool lock_acquired;
    int32_t lock_error;

    if (initially_unlocked != FT_TRUE && initially_unlocked != FT_FALSE)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }

    lock_acquired = FT_FALSE;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::register_recipe");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    lock_error = copied_ingredients.initialize();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(copied_ingredients.get_error());
        return (copied_ingredients.get_error());
    }
    ingredient_index = 0;
    ingredient_count = ingredients.size();
    while (ingredient_index < ingredient_count)
    {
        lock_error = copied_ingredients.push_back(ingredients[ingredient_index]);
        if (lock_error != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            this->set_error(lock_error);
            return (lock_error);
        }
        ingredient_index++;
    }
    this->_recipes.insert(recipe_id, copied_ingredients);
    if (this->_recipes.get_error() != FT_ERR_SUCCESS)
    {
        lock_error = this->_recipes.get_error();
        this->unlock_internal(lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_unlocked_recipes.insert(recipe_id, initially_unlocked);
    if (this->_unlocked_recipes.get_error() != FT_ERR_SUCCESS)
    {
        lock_error = this->_unlocked_recipes.get_error();
        this->_recipes.remove(recipe_id);
        this->unlock_internal(lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::register_recipe(int32_t recipe_id,
    ft_vector<game_crafting_ingredient> &&ingredients,
    const ft_sharedptr<game_item> &output_definition,
    int32_t output_quantity, ft_bool initially_unlocked) noexcept
{
    ft_sharedptr<game_item> owned_definition;
    game_crafting_output output;
    ft_bool lock_acquired;
    int32_t result;

    if (!output_definition || output_quantity <= 0
        || output_definition->get_max_stack() <= 0
        || (initially_unlocked != FT_TRUE && initially_unlocked != FT_FALSE))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = owned_definition.initialize(new (std::nothrow) game_item());
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (!owned_definition)
        return (this->set_error(FT_ERR_NO_MEMORY));
    result = owned_definition->initialize(*output_definition);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    lock_acquired = FT_FALSE;
    result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = this->register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        initially_unlocked);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (result);
    }
    output.item_definition = owned_definition;
    output.quantity = output_quantity;
    this->_recipe_outputs.insert(recipe_id, output);
    result = this->_recipe_outputs.get_error();
    if (result != FT_ERR_SUCCESS)
    {
        this->_recipes.remove(recipe_id);
        this->_unlocked_recipes.remove(recipe_id);
        this->unlock_internal(lock_acquired);
        return (this->set_error(result));
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_crafting::unlock_recipe(int32_t recipe_id) noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::unlock_recipe");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    recipe_entry = this->_recipes.find(recipe_id);
    if (recipe_entry == this->_recipes.end())
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    if (unlock_entry == this->_unlocked_recipes.end())
        this->_unlocked_recipes.insert(recipe_id, FT_TRUE);
    else
        unlock_entry->value = FT_TRUE;
    lock_error = this->_unlocked_recipes.get_error();
    this->unlock_internal(lock_acquired);
    this->set_error(lock_error);
    return (lock_error);
}

int32_t game_crafting::is_recipe_unlocked(int32_t recipe_id,
    ft_bool *unlocked) const noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    game_crafting *self;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::is_recipe_unlocked");
    if (unlocked == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    *unlocked = FT_FALSE;
    self = const_cast<game_crafting *>(this);
    lock_acquired = FT_FALSE;
    lock_error = self->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    recipe_entry = self->_recipes.find(recipe_id);
    if (recipe_entry == self->_recipes.end())
    {
        self->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    unlock_entry = self->_unlocked_recipes.find(recipe_id);
    if (unlock_entry != self->_unlocked_recipes.end())
        *unlocked = static_cast<ft_bool>(unlock_entry->value);
    self->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::query_recipe(int32_t recipe_id,
    const game_inventory &inventory, game_crafting_query_result *query_result,
    ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    const Pair<int32_t, ft_sharedptr<game_item>> *item_entry;
    ft_vector<game_crafting_missing_ingredient> all_groups;
    ft_size_t index;
    ft_size_t prior_index;
    ft_size_t item_index;
    ft_size_t item_count;
    int64_t available;
    int64_t consumed;
    int64_t required;
    ft_bool crafting_locked;
    ft_bool inventory_locked;
    int32_t result;

    if (query_result == ft_nullptr || missing_groups == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    query_result->found = FT_FALSE;
    query_result->known = FT_FALSE;
    query_result->craftable = FT_FALSE;
    query_result->missing_group_count = 0U;
    query_result->missing_units = 0;
    if (missing_groups->is_initialised() == FT_CLASS_STATE_INITIALISED)
        missing_groups->clear();
    else
    {
        result = missing_groups->initialize();
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::query_recipe");
    if (inventory._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);

    crafting_locked = FT_FALSE;
    inventory_locked = FT_FALSE;
    result = this->lock_internal(&crafting_locked);
    if (result != FT_ERR_SUCCESS)
        return (result);
    recipe_entry = this->_recipes.find(recipe_id);
    if (recipe_entry == this->_recipes.end())
    {
        this->unlock_internal(crafting_locked);
        return (FT_ERR_NOT_FOUND);
    }
    query_result->found = FT_TRUE;
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    query_result->known = (unlock_entry != this->_unlocked_recipes.end()
        && unlock_entry->value == FT_TRUE) ? FT_TRUE : FT_FALSE;
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(crafting_locked);
        query_result->found = FT_FALSE;
        query_result->known = FT_FALSE;
        return (result);
    }
    result = all_groups.initialize();
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < recipe_entry->value.size())
    {
        game_crafting_missing_ingredient group;
        const game_crafting_ingredient &ingredient = recipe_entry->value[index];

        group.item_id = ingredient.get_item_id();
        group.rarity = ingredient.get_rarity();
        group.required_units = ingredient.get_count();
        group.available_units = 0;
        group.missing_units = 0;
        if (group.item_id <= 0 || group.required_units <= 0
            || group.rarity < -1)
        {
            result = FT_ERR_CONFIGURATION;
            break ;
        }
        result = all_groups.push_back(group);
        index++;
    }
    /* Allocate rarity-specific requirements first. Wildcard groups then use
     * only the remaining units, so duplicate groups cannot reuse a stack. */
    for (int32_t pass = 0; pass < 2 && result == FT_ERR_SUCCESS; pass++)
    {
        index = 0U;
        while (index < all_groups.size() && result == FT_ERR_SUCCESS)
        {
            game_crafting_missing_ingredient &group = all_groups[index];
            const ft_bool wildcard = group.rarity == -1
                ? FT_TRUE : FT_FALSE;

            if ((pass == 0 && wildcard == FT_TRUE)
                || (pass == 1 && wildcard == FT_FALSE))
            {
                index++;
                continue;
            }
            available = 0;
            item_count = inventory._items.size();
            item_entry = inventory._items.end() - item_count;
            item_index = 0U;
            while (item_index < item_count)
            {
                if (item_entry->value
                    && item_entry->value->get_item_id() == group.item_id
                    && (wildcard == FT_TRUE
                        || item_entry->value->get_rarity() == group.rarity))
                {
                    available += item_entry->value->get_stack_size();
                    if (available > FT_INT32_MAX)
                        available = FT_INT32_MAX;
                }
                item_entry++;
                item_index++;
            }
            consumed = 0;
            prior_index = 0U;
            while (prior_index < all_groups.size())
            {
                const game_crafting_missing_ingredient &prior = all_groups[prior_index];
                const ft_bool prior_wildcard = prior.rarity == -1
                    ? FT_TRUE : FT_FALSE;
                if (prior_index != index && prior.item_id == group.item_id
                    && prior.available_units > 0
                    && ((wildcard == FT_FALSE && prior_wildcard == FT_FALSE
                            && prior.rarity == group.rarity)
                        || (wildcard == FT_TRUE
                            && ((prior_wildcard == FT_TRUE)
                                || (pass == 1 && prior_wildcard == FT_FALSE)))))
                    consumed += prior.available_units;
                prior_index++;
            }
            if (consumed > available)
                consumed = available;
            available -= consumed;
            required = group.required_units;
            if (available > required)
                available = required;
            group.available_units = static_cast<int32_t>(available);
            group.missing_units = static_cast<int32_t>(required - available);
            index++;
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        index = 0U;
        while (index < all_groups.size() && result == FT_ERR_SUCCESS)
        {
            const game_crafting_missing_ingredient &group = all_groups[index];
            if (group.missing_units > 0)
            {
                result = missing_groups->push_back(group);
                if (result == FT_ERR_SUCCESS)
                {
                    query_result->missing_group_count++;
                    if (query_result->missing_units
                        > FT_INT32_MAX - group.missing_units)
                        query_result->missing_units = FT_INT32_MAX;
                    else
                        query_result->missing_units += group.missing_units;
                }
            }
            index++;
        }
    }
    inventory.unlock(inventory_locked);
    this->unlock_internal(crafting_locked);
    if (result != FT_ERR_SUCCESS)
    {
        query_result->found = FT_FALSE;
        query_result->known = FT_FALSE;
        query_result->missing_group_count = 0U;
        query_result->missing_units = 0;
        missing_groups->clear();
        return (result);
    }
    query_result->craftable = (query_result->known == FT_TRUE
        && query_result->missing_group_count == 0U) ? FT_TRUE : FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static int32_t game_crafting_clone_item(
    const ft_sharedptr<game_item> &source,
    ft_sharedptr<game_item> *destination) noexcept
{
    if (!source || destination == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t result = destination->initialize(new (std::nothrow) game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (!*destination)
        return (FT_ERR_NO_MEMORY);
    return ((*destination)->initialize(*source));
}

static ft_bool game_crafting_items_equal(const game_item &left,
    const game_item &right) noexcept
{
    return (left.get_item_id() == right.get_item_id()
        && left.get_stack_size() == right.get_stack_size()
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
        && left.get_modifier4_value() == right.get_modifier4_value());
}

int32_t game_crafting::prepare_craft(const game_inventory &inventory,
    int32_t recipe_id, int32_t batch_count,
    game_crafting_prepared *prepared) noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    Pair<int32_t, game_crafting_output> *output_entry;
    game_inventory staged;
    game_crafting_prepared candidate;
    ft_size_t index;
    ft_size_t item_count;
    const Pair<int32_t, ft_sharedptr<game_item>> *entry;
    ft_bool crafting_locked;
    ft_bool inventory_locked;
    int32_t result;

    if (prepared == ft_nullptr || batch_count <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (prepared->_initialised_state == FT_CLASS_STATE_INITIALISED)
        result = prepared->clear();
    else
        result = prepared->initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::prepare_craft");
    crafting_locked = FT_FALSE;
    inventory_locked = FT_FALSE;
    result = this->lock_internal(&crafting_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    recipe_entry = this->_recipes.find(recipe_id);
    if (recipe_entry == this->_recipes.end())
        result = FT_ERR_NOT_FOUND;
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    if (result == FT_ERR_SUCCESS
        && (unlock_entry == this->_unlocked_recipes.end()
            || unlock_entry->value != FT_TRUE))
        result = FT_ERR_INVALID_STATE;
    output_entry = this->_recipe_outputs.find(recipe_id);
    if (result == FT_ERR_SUCCESS && output_entry == this->_recipe_outputs.end())
        result = FT_ERR_NOT_FOUND;
    if (result == FT_ERR_SUCCESS
        && output_entry->value.quantity > FT_INT32_MAX / batch_count)
        result = FT_ERR_INVALID_ARGUMENT;
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(result));
    }
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(result));
    }
    result = candidate.initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged.initialize(inventory._capacity, inventory._weight_limit);
    if (result != FT_ERR_SUCCESS)
    {
        inventory.unlock(inventory_locked);
        this->unlock_internal(crafting_locked);
        return (this->set_error(result));
    }
    index = 0U;
    item_count = inventory._items.size();
    entry = inventory._items.end() - item_count;
    while (index < item_count && result == FT_ERR_SUCCESS)
    {
        ft_sharedptr<game_item> expected_copy;
        ft_sharedptr<game_item> staged_copy;

        if (entry->value)
        {
            result = game_crafting_clone_item(entry->value, &expected_copy);
            if (result == FT_ERR_SUCCESS)
                result = game_crafting_clone_item(entry->value, &staged_copy);
            if (result == FT_ERR_SUCCESS)
            {
                candidate._expected_items.insert(entry->key, expected_copy);
                result = candidate._expected_items.get_error();
            }
            if (result == FT_ERR_SUCCESS)
            {
                staged._items.insert(entry->key, staged_copy);
                result = staged._items.get_error();
            }
        }
        entry++;
        index++;
    }
    staged._used_slots = inventory._used_slots;
    staged._current_weight = inventory._current_weight;
    staged._next_slot = inventory._next_slot;
    if (result == FT_ERR_SUCCESS)
    {
        candidate._expected_capacity = inventory._capacity;
        candidate._expected_used_slots = inventory._used_slots;
        candidate._expected_weight_limit = inventory._weight_limit;
        candidate._expected_current_weight = inventory._current_weight;
        candidate._expected_next_slot = inventory._next_slot;
        candidate._result_capacity = inventory._capacity;
        candidate._result_weight_limit = inventory._weight_limit;
        candidate._result_used_slots = inventory._used_slots;
        candidate._result_current_weight = inventory._current_weight;
        candidate._result_next_slot = inventory._next_slot;
    }
    index = 0U;
    while (result == FT_ERR_SUCCESS
        && index < recipe_entry->value.size())
    {
        game_crafting_ingredient &ingredient = recipe_entry->value[index];
        int32_t per_craft = ingredient.get_count();
        int32_t remaining;
        int32_t slot;

        if (per_craft <= 0 || per_craft > FT_INT32_MAX / batch_count)
        {
            result = FT_ERR_INVALID_ARGUMENT;
            break;
        }
        remaining = per_craft * batch_count;
        slot = 0;
        while (slot < staged._next_slot && remaining > 0)
        {
            Pair<int32_t, ft_sharedptr<game_item>> *item_entry;

            item_entry = staged._items.find(slot);
            if (item_entry != staged._items.end() && item_entry->value
                && item_entry->value->get_item_id() == ingredient.get_item_id()
                && (ingredient.get_rarity() == -1
                    || item_entry->value->get_rarity() == ingredient.get_rarity()))
            {
                int32_t consumed = item_entry->value->get_stack_size();
                if (consumed > remaining)
                    consumed = remaining;
                item_entry->value->sub_from_stack(consumed);
                staged._current_weight -= consumed;
                remaining -= consumed;
                if (item_entry->value->get_stack_size() == 0)
                    staged.remove_item(slot);
            }
            slot++;
        }
        if (remaining > 0)
            result = FT_ERR_GAME_GENERAL_ERROR;
        index++;
    }
    if (result == FT_ERR_SUCCESS)
    {
        ft_sharedptr<game_item> output_copy;
        result = game_crafting_clone_item(output_entry->value.item_definition,
            &output_copy);
        if (result == FT_ERR_SUCCESS)
        {
            output_copy->set_stack_size(output_entry->value.quantity * batch_count);
            result = staged.add_item(output_copy);
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        candidate._result_capacity = staged._capacity;
        candidate._result_used_slots = staged._used_slots;
        candidate._result_weight_limit = staged._weight_limit;
        candidate._result_current_weight = staged._current_weight;
        candidate._result_next_slot = staged._next_slot;
        result = candidate._result_items.move_from(staged._items);
    }
    if (result == FT_ERR_SUCCESS)
    {
        candidate._ready = FT_TRUE;
        result = prepared->_expected_items.move_from(candidate._expected_items);
        if (result == FT_ERR_SUCCESS)
            result = prepared->_result_items.move_from(candidate._result_items);
        if (result == FT_ERR_SUCCESS)
        {
            prepared->_expected_capacity = candidate._expected_capacity;
            prepared->_expected_used_slots = candidate._expected_used_slots;
            prepared->_expected_weight_limit = candidate._expected_weight_limit;
            prepared->_expected_current_weight = candidate._expected_current_weight;
            prepared->_expected_next_slot = candidate._expected_next_slot;
            prepared->_result_capacity = candidate._result_capacity;
            prepared->_result_used_slots = candidate._result_used_slots;
            prepared->_result_weight_limit = candidate._result_weight_limit;
            prepared->_result_current_weight = candidate._result_current_weight;
            prepared->_result_next_slot = candidate._result_next_slot;
            prepared->_ready = FT_TRUE;
        }
    }
    inventory.unlock(inventory_locked);
    this->unlock_internal(crafting_locked);
    return (this->set_error(result));
}

int32_t game_crafting::commit_craft(game_inventory &inventory,
    game_crafting_prepared *prepared) noexcept
{
    ft_size_t index;
    ft_size_t count;
    const Pair<int32_t, ft_sharedptr<game_item>> *expected;
    ft_bool inventory_locked;
    int32_t result;

    if (prepared == ft_nullptr
        || prepared->_initialised_state != FT_CLASS_STATE_INITIALISED
        || prepared->_ready != FT_TRUE)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    inventory_locked = FT_FALSE;
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (inventory._capacity != prepared->_expected_capacity
        || inventory._used_slots != prepared->_expected_used_slots
        || inventory._weight_limit != prepared->_expected_weight_limit
        || inventory._current_weight != prepared->_expected_current_weight
        || inventory._next_slot != prepared->_expected_next_slot
        || inventory._items.size() != prepared->_expected_items.size())
        result = FT_ERR_INVALID_STATE;
    index = 0U;
    count = prepared->_expected_items.size();
    expected = prepared->_expected_items.end() - count;
    while (result == FT_ERR_SUCCESS && index < count)
    {
        const Pair<int32_t, ft_sharedptr<game_item>> *actual =
            inventory._items.find(expected->key);
        if (actual == inventory._items.end() || !actual->value
            || !expected->value
            || game_crafting_items_equal(*actual->value, *expected->value) != FT_TRUE)
            result = FT_ERR_INVALID_STATE;
        expected++;
        index++;
    }
    if (result == FT_ERR_SUCCESS)
        result = inventory._items.move_from(prepared->_result_items);
    if (result == FT_ERR_SUCCESS)
    {
        inventory._capacity = prepared->_result_capacity;
        inventory._used_slots = prepared->_result_used_slots;
        inventory._weight_limit = prepared->_result_weight_limit;
        inventory._current_weight = prepared->_result_current_weight;
        inventory._next_slot = prepared->_result_next_slot;
        prepared->_ready = FT_FALSE;
    }
    inventory.unlock(inventory_locked);
    return (this->set_error(result));
}

int32_t game_crafting::craft_item(game_inventory &inventory, int32_t recipe_id,
    const ft_sharedptr<game_item> &result) noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    ft_vector<game_crafting_ingredient> *ingredients;
    game_inventory staged_inventory;
    ft_size_t ingredient_index;
    ft_size_t source_item_count;
    ft_size_t source_item_index;
    Pair<int32_t, ft_sharedptr<game_item>> *source_item;
    ft_bool inventory_lock_acquired;
    ft_bool lock_acquired;
    int32_t lock_error;

    if (!result)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::craft_item");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    recipe_entry = this->_recipes.find(recipe_id);
    if (recipe_entry == this->_recipes.end())
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    if (unlock_entry == this->_unlocked_recipes.end()
        || unlock_entry->value != FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    ingredients = &recipe_entry->value;

    inventory_lock_acquired = FT_FALSE;
    lock_error = inventory.lock(&inventory_lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    if (this->_recipe_outputs.find(recipe_id) != this->_recipe_outputs.end())
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = staged_inventory.initialize(inventory._capacity,
        inventory._weight_limit);
    if (lock_error != FT_ERR_SUCCESS)
    {
        inventory.unlock(inventory_lock_acquired);
        this->unlock_internal(lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    source_item_count = inventory._items.size();
    source_item = inventory._items.end() - source_item_count;
    source_item_index = 0;
    while (source_item_index < source_item_count)
    {
        if (source_item->value)
        {
            ft_sharedptr<game_item> item_copy(new game_item());
            if (!item_copy)
                lock_error = FT_ERR_NO_MEMORY;
            else
                lock_error = item_copy->initialize(*source_item->value);
            if (lock_error == FT_ERR_SUCCESS)
                lock_error = staged_inventory.add_item(item_copy);
            if (lock_error != FT_ERR_SUCCESS)
            {
                inventory.unlock(inventory_lock_acquired);
                this->unlock_internal(lock_acquired);
                this->set_error(lock_error);
                return (lock_error);
            }
        }
        source_item++;
        source_item_index++;
    }
    ingredient_index = 0;
    while (ingredient_index < ingredients->size())
    {
        game_crafting_ingredient &ingredient = (*ingredients)[ingredient_index];
        int32_t remaining;
        int32_t slot_index;

        remaining = ingredient.get_count();
        if (remaining < 0)
        {
            inventory.unlock(inventory_lock_acquired);
            this->unlock_internal(lock_acquired);
            this->set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_ARGUMENT);
        }
        slot_index = 0;
        while (slot_index < staged_inventory._next_slot && remaining > 0)
        {
            Pair<int32_t, ft_sharedptr<game_item>> *item_entry;

            item_entry = staged_inventory._items.find(slot_index);
            if (item_entry != staged_inventory._items.end()
                && item_entry->value
                && item_entry->value->get_item_id() == ingredient.get_item_id()
                && (ingredient.get_rarity() == -1
                    || item_entry->value->get_rarity() == ingredient.get_rarity()))
            {
                int32_t remove_count;

                remove_count = item_entry->value->get_stack_size();
                if (remove_count > remaining)
                    remove_count = remaining;
                item_entry->value->sub_from_stack(remove_count);
                remaining -= remove_count;
                if (item_entry->value->get_stack_size() == 0)
                    staged_inventory.remove_item(item_entry->key);
            }
            slot_index++;
        }
        if (remaining > 0)
        {
            inventory.unlock(inventory_lock_acquired);
            this->unlock_internal(lock_acquired);
            this->set_error(FT_ERR_GAME_GENERAL_ERROR);
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        ingredient_index++;
    }
    lock_error = staged_inventory.add_item(result);
    if (lock_error != FT_ERR_SUCCESS)
    {
        inventory.unlock(inventory_lock_acquired);
        this->unlock_internal(lock_acquired);
        this->set_error(lock_error);
        return (lock_error);
    }
    lock_error = inventory._items.move_from(staged_inventory._items);
    if (lock_error == FT_ERR_SUCCESS)
    {
        inventory._capacity = staged_inventory._capacity;
        inventory._used_slots = staged_inventory._used_slots;
        inventory._current_weight = staged_inventory._current_weight;
        inventory._next_slot = staged_inventory._next_slot;
    }
    inventory.unlock(inventory_lock_acquired);
    this->unlock_internal(lock_acquired);
    this->set_error(lock_error);
    return (lock_error);
}
