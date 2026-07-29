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
    : _recipes(), _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
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
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t recipes_error = this->_recipes.destroy();
    int32_t disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (recipes_error != FT_ERR_SUCCESS)
    {
        this->set_error(recipes_error);
        return (recipes_error);
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
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._recipes.clear();
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
    ft_vector<game_crafting_ingredient> copied_ingredients;
    ft_size_t ingredient_index;
    ft_size_t ingredient_count;
    ft_bool lock_acquired;
    int32_t lock_error;

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
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::craft_item(game_inventory &inventory, int32_t recipe_id,
    const ft_sharedptr<game_item> &result) noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    ft_vector<game_crafting_ingredient> *ingredients;
    ft_size_t ingredient_index;
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
    ingredients = &recipe_entry->value;
    ingredient_index = 0;
    while (ingredient_index < ingredients->size())
    {
        game_crafting_ingredient &ingredient = (*ingredients)[ingredient_index];
        int32_t have_count;

        if (ingredient.get_rarity() == -1)
            have_count = inventory.count_item(ingredient.get_item_id());
        else
            have_count = inventory.count_rarity(ingredient.get_rarity());
        if (have_count < ingredient.get_count())
        {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
        }
        ingredient_index++;
    }
    ingredient_index = 0;
    while (ingredient_index < ingredients->size())
    {
        game_crafting_ingredient &ingredient = (*ingredients)[ingredient_index];
        ft_map<int32_t, ft_sharedptr<game_item> > &items = inventory.get_items();
        Pair<int32_t, ft_sharedptr<game_item> > *item_ptr;
        Pair<int32_t, ft_sharedptr<game_item> > *item_end;
        int32_t remaining;

        remaining = ingredient.get_count();
        item_ptr = items.end() - items.size();
        item_end = items.end();
        while (item_ptr != item_end && remaining > 0)
        {
            if (item_ptr->value)
            {
                if (item_ptr->value->get_item_id() == ingredient.get_item_id())
                {
                    if (ingredient.get_rarity() == -1
                        || item_ptr->value->get_rarity() == ingredient.get_rarity())
                    {
                        int32_t remove_count;

                        remove_count = item_ptr->value->get_stack_size();
                        if (remove_count > remaining)
                            remove_count = remaining;
                        item_ptr->value->sub_from_stack(remove_count);
                        remaining -= remove_count;
                        if (item_ptr->value->get_stack_size() == 0)
                            items.remove(item_ptr->key);
                    }
                }
            }
            item_ptr++;
        }
        ingredient_index++;
    }
    this->unlock_internal(lock_acquired);
    int32_t result_error = inventory.add_item(result);
    this->set_error(result_error);
    return (result_error);
}
