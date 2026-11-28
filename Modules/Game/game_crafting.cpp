#include "../Printf/printf.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "game_crafting.hpp"
#include <algorithm>
#include <new>
#include <limits>
#include <stdexcept>
#include <vector>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"

thread_local int32_t game_crafting_ingredient::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_crafting::_last_error = FT_ERR_SUCCESS;

static int32_t game_crafting_clone_item(
    const ft_sharedptr<game_item> &source,
    ft_sharedptr<game_item> *destination) noexcept;

game_crafting_prepared::game_crafting_prepared() noexcept
    : _expected_items(), _result_items(), _expected_capacity(0U),
      _expected_used_slots(0U), _expected_weight_limit(0),
      _expected_current_weight(0), _expected_next_slot(0),
      _expected_revision(0U),
      _result_capacity(0U), _result_used_slots(0U), _result_weight_limit(0),
      _result_current_weight(0), _result_next_slot(0),
      _primary_output_slot(-1), _primary_output_item_id(0),
      _primary_output_quantity(0),
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
    this->_primary_output_slot = -1;
    this->_primary_output_item_id = 0;
    this->_primary_output_quantity = 0;
    this->_ready = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_prepared::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->_expected_items.destroy();
    (void)this->_result_items.destroy();
    this->_primary_output_slot = -1;
    this->_primary_output_item_id = 0;
    this->_primary_output_quantity = 0;
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
    : _item_id(0), _tag_id(0), _count(0), _rarity(-1), _alternative_item_ids{0},
      _alternative_count(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    return ;
}

game_crafting_ingredient::game_crafting_ingredient(
    const game_crafting_ingredient &other) noexcept
    : _item_id(0), _tag_id(0), _count(0), _rarity(-1), _alternative_item_ids{0},
      _alternative_count(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    (void)this->initialize(other);
    return ;
}

game_crafting_ingredient::game_crafting_ingredient(
    game_crafting_ingredient &&other) noexcept
    : _item_id(0), _tag_id(0), _count(0), _rarity(-1), _alternative_item_ids{0},
      _alternative_count(0U),
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
    : _recipes(), _unlocked_recipes(), _recipe_outputs(), _recipe_stations(),
      _item_tag_ids{0}, _item_tag_member_counts{0}, _item_tag_members{{0}},
      _item_tag_count(0U),
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
    recipes_error = this->_recipe_stations.initialize();
    if (recipes_error != FT_ERR_SUCCESS)
    {
        (void)this->_recipe_outputs.destroy();
        (void)this->_unlocked_recipes.destroy();
        (void)this->_recipes.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(recipes_error);
        return (recipes_error);
    }
    this->_item_tag_count = 0U;
    uint32_t tag_index = 0U;
    while (tag_index < MAXIMUM_ITEM_TAGS)
    {
        this->_item_tag_ids[tag_index] = 0;
        this->_item_tag_member_counts[tag_index] = 0U;
        uint32_t member_index = 0U;
        while (member_index < MAXIMUM_TAG_MEMBERS)
        {
            this->_item_tag_members[tag_index][member_index] = 0;
            ++member_index;
        }
        ++tag_index;
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
    int32_t stations_error = this->_recipe_stations.destroy();
    int32_t disable_error = this->disable_thread_safety();
    this->_item_tag_count = 0U;
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
    if (stations_error != FT_ERR_SUCCESS)
        return (this->set_error(stations_error));
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
    this->_recipe_stations = other._recipe_stations;
    this->_item_tag_count = other._item_tag_count;
    uint32_t tag_index = 0U;
    while (tag_index < MAXIMUM_ITEM_TAGS)
    {
        this->_item_tag_ids[tag_index] = other._item_tag_ids[tag_index];
        this->_item_tag_member_counts[tag_index] =
            other._item_tag_member_counts[tag_index];
        uint32_t member_index = 0U;
        while (member_index < MAXIMUM_TAG_MEMBERS)
        {
            this->_item_tag_members[tag_index][member_index] =
                other._item_tag_members[tag_index][member_index];
            ++member_index;
        }
        ++tag_index;
    }
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._recipes.clear();
    other._unlocked_recipes.clear();
    other._recipe_outputs.clear();
    other._recipe_stations.clear();
    other._item_tag_count = 0U;
    tag_index = 0U;
    while (tag_index < MAXIMUM_ITEM_TAGS)
    {
        other._item_tag_ids[tag_index] = 0;
        other._item_tag_member_counts[tag_index] = 0U;
        uint32_t member_index = 0U;
        while (member_index < MAXIMUM_TAG_MEMBERS)
        {
            other._item_tag_members[tag_index][member_index] = 0;
            ++member_index;
        }
        ++tag_index;
    }
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
    this->_tag_id = other._tag_id;
    this->_count = other._count;
    this->_rarity = other._rarity;
    this->_alternative_count = other._alternative_count;
    for (uint32_t alt_index = 0U; alt_index < MAX_ALTERNATIVE_ITEM_IDS; ++alt_index)
        this->_alternative_item_ids[alt_index] = other._alternative_item_ids[alt_index];
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
    other._tag_id = 0;
    other._count = 0;
    other._rarity = -1;
    other._alternative_count = 0U;
    for (uint32_t alt_index = 0U; alt_index < MAX_ALTERNATIVE_ITEM_IDS; ++alt_index)
        other._alternative_item_ids[alt_index] = 0;
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
    if (item_id <= 0 || count <= 0 || rarity < -1)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
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
    this->_tag_id = 0;
    this->_count = count;
    this->_rarity = rarity;
    this->_alternative_count = 1U;
    for (uint32_t alt_index = 0U; alt_index < MAX_ALTERNATIVE_ITEM_IDS; ++alt_index)
        this->_alternative_item_ids[alt_index] = 0;
    this->_alternative_item_ids[0] = item_id;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting_ingredient::initialize_alternatives(
    const int32_t *item_ids, uint32_t item_id_count, int32_t count,
    int32_t rarity) noexcept
{
    if (item_ids == ft_nullptr || item_id_count == 0U
        || item_id_count > MAX_ALTERNATIVE_ITEM_IDS || count <= 0 || rarity < -1)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    uint32_t index = 0U;
    while (index < item_id_count)
    {
        if (item_ids[index] <= 0)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        uint32_t prior = 0U;
        while (prior < index)
        {
            if (item_ids[prior] == item_ids[index])
                return (this->set_error(FT_ERR_INVALID_ARGUMENT));
            ++prior;
        }
        ++index;
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_item_id = item_ids[0];
    this->_tag_id = 0;
    this->_count = count;
    this->_rarity = rarity;
    this->_alternative_count = item_id_count;
    index = 0U;
    while (index < MAX_ALTERNATIVE_ITEM_IDS)
    {
        if (index < item_id_count)
            this->_alternative_item_ids[index] = item_ids[index];
        else
            this->_alternative_item_ids[index] = 0;
        ++index;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_crafting_ingredient::initialize_tag(int32_t tag_id,
    int32_t count, int32_t rarity) noexcept
{
    if (tag_id <= 0 || count <= 0 || rarity < -1)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_item_id = 0;
    this->_tag_id = tag_id;
    this->_count = count;
    this->_rarity = rarity;
    this->_alternative_count = 0U;
    uint32_t index = 0U;
    while (index < MAX_ALTERNATIVE_ITEM_IDS)
    {
        this->_alternative_item_ids[index] = 0;
        ++index;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_crafting_ingredient::get_tag_id() const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (0);
    const int32_t value = this->_tag_id;
    this->unlock_internal(lock_acquired);
    return (value);
}

uint32_t game_crafting_ingredient::get_alternative_count() const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (0U);
    uint32_t count = this->_alternative_count;
    this->unlock_internal(lock_acquired);
    return (count);
}

int32_t game_crafting_ingredient::get_alternative_item_id(uint32_t index) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (index >= MAX_ALTERNATIVE_ITEM_IDS || this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (0);
    int32_t item_id = index < this->_alternative_count ? this->_alternative_item_ids[index] : 0;
    this->unlock_internal(lock_acquired);
    return (item_id);
}

ft_bool game_crafting_ingredient::matches_item_id(int32_t item_id) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (item_id <= 0 || this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    uint32_t index = 0U;
    while (index < this->_alternative_count)
    {
        if (this->_alternative_item_ids[index] == item_id)
        {
            this->unlock_internal(lock_acquired);
            return (FT_TRUE);
        }
        ++index;
    }
    this->unlock_internal(lock_acquired);
    return (FT_FALSE);
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
    this->_tag_id = 0;
    this->_count = 0;
    this->_rarity = -1;
    this->_alternative_count = 0U;
    for (uint32_t alt_index = 0U; alt_index < MAX_ALTERNATIVE_ITEM_IDS; ++alt_index)
        this->_alternative_item_ids[alt_index] = 0;
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
    this->_tag_id = 0;
    this->_alternative_count = item_id > 0 ? 1U : 0U;
    for (uint32_t alt_index = 0U; alt_index < MAX_ALTERNATIVE_ITEM_IDS; ++alt_index)
        this->_alternative_item_ids[alt_index] = 0;
    if (item_id > 0)
        this->_alternative_item_ids[0] = item_id;
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

int32_t game_crafting::register_item_tag(int32_t tag_id,
    const int32_t *item_ids, uint32_t item_id_count) noexcept
{
    if (tag_id <= 0 || item_ids == ft_nullptr || item_id_count == 0U
        || item_id_count > MAXIMUM_TAG_MEMBERS)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::register_item_tag");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (this->_recipes.size() != 0U)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_INVALID_OPERATION));
    }
    if (this->_item_tag_count >= MAXIMUM_ITEM_TAGS)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_FULL));
    }
    uint32_t index = 0U;
    while (index < this->_item_tag_count)
    {
        if (this->_item_tag_ids[index] == tag_id)
        {
            this->unlock_internal(lock_acquired);
            return (this->set_error(FT_ERR_ALREADY_EXISTS));
        }
        ++index;
    }
    index = 0U;
    while (index < item_id_count)
    {
        if (item_ids[index] <= 0)
        {
            this->unlock_internal(lock_acquired);
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        }
        uint32_t previous = 0U;
        while (previous < index)
        {
            if (item_ids[previous] == item_ids[index])
            {
                this->unlock_internal(lock_acquired);
                return (this->set_error(FT_ERR_INVALID_ARGUMENT));
            }
            ++previous;
        }
        ++index;
    }
    const uint32_t destination = this->_item_tag_count;
    this->_item_tag_ids[destination] = tag_id;
    this->_item_tag_member_counts[destination] = item_id_count;
    index = 0U;
    while (index < MAXIMUM_TAG_MEMBERS)
    {
        if (index < item_id_count)
            this->_item_tag_members[destination][index] = item_ids[index];
        else
            this->_item_tag_members[destination][index] = 0;
        ++index;
    }
    this->_item_tag_count += 1U;
    this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
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
        const game_crafting_ingredient &source = ingredients[ingredient_index];
        const int32_t tag_id = source.get_tag_id();
        if (tag_id > 0)
        {
            uint32_t tag_index = 0U;
            while (tag_index < this->_item_tag_count
                && this->_item_tag_ids[tag_index] != tag_id)
                ++tag_index;
            if (tag_index == this->_item_tag_count)
            {
                this->unlock_internal(lock_acquired);
                return (this->set_error(FT_ERR_NOT_FOUND));
            }
            game_crafting_ingredient expanded;
            lock_error = expanded.initialize_alternatives(
                this->_item_tag_members[tag_index],
                this->_item_tag_member_counts[tag_index], source.get_count(),
                source.get_rarity());
            if (lock_error == FT_ERR_SUCCESS)
                lock_error = copied_ingredients.push_back(expanded);
        }
        else
            lock_error = copied_ingredients.push_back(source);
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
    const std::vector<game_crafting_byproduct> byproducts;

    return (this->register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output_definition, output_quantity, byproducts, initially_unlocked));
}

int32_t game_crafting::register_recipe(int32_t recipe_id,
    ft_vector<game_crafting_ingredient> &&ingredients,
    const ft_sharedptr<game_item> &output_definition,
    int32_t output_quantity,
    const std::vector<game_crafting_byproduct> &byproducts,
    ft_bool initially_unlocked) noexcept
{
    game_crafting_output_snapshot output = {};
    game_inventory_item_snapshot definition;
    ft_size_t byproduct_index;
    ft_bool lock_acquired;
    int32_t result;

    if (!output_definition || output_quantity <= 0
        || output_definition->_initialised_state != FT_CLASS_STATE_INITIALISED
        || byproducts.size() > GAME_CRAFTING_MAX_BYPRODUCTS
        || (initially_unlocked != FT_TRUE && initially_unlocked != FT_FALSE))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = output_definition->capture_inventory_fields(0, definition);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (definition.instance_id != 0U || definition.max_stack <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    output.primary = {definition.item_id, output_quantity, definition.max_stack,
        definition.rarity, definition.width, definition.height,
        definition.modifier1_id, definition.modifier1_value,
        definition.modifier2_id, definition.modifier2_value,
        definition.modifier3_id, definition.modifier3_value,
        definition.modifier4_id, definition.modifier4_value, definition.instance_id};
    output.byproduct_count = static_cast<uint32_t>(byproducts.size());
    byproduct_index = 0U;
    while (byproduct_index < byproducts.size())
    {
        const game_crafting_byproduct &source = byproducts[byproduct_index];

        if (source.item_id <= 0 || source.quantity <= 0
            || source.max_stack <= 0 || source.instance_id != 0U
            || source.width <= 0 || source.height <= 0)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        output.byproducts[byproduct_index] = {source.item_id, source.quantity, source.max_stack,
            source.rarity, source.width, source.height,
            source.modifier1_id, source.modifier1_value, source.modifier2_id, source.modifier2_value,
            source.modifier3_id, source.modifier3_value, source.modifier4_id, source.modifier4_value,
            source.instance_id};
        byproduct_index++;
    }
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

int32_t game_crafting::lock_recipe(int32_t recipe_id) noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::lock_recipe");
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
        this->_unlocked_recipes.insert(recipe_id, FT_FALSE);
    else
        unlock_entry->value = FT_FALSE;
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
    return (this->query_recipe_at_station(recipe_id, inventory, 0,
        query_result, missing_groups));
}

int32_t game_crafting::set_recipe_station(int32_t recipe_id,
    int32_t station_id) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result;
    if (recipe_id <= 0 || station_id <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::set_recipe_station");
    result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (this->_recipes.find(recipe_id) == this->_recipes.end())
        result = FT_ERR_NOT_FOUND;
    else if (this->_recipe_stations.find(recipe_id)
        != this->_recipe_stations.end())
        result = FT_ERR_ALREADY_EXISTS;
    else
    {
        const Pair<int32_t, ft_bool> *unlock_entry =
            this->_unlocked_recipes.find(recipe_id);

        if (unlock_entry == this->_unlocked_recipes.end()
            || unlock_entry->value == FT_TRUE)
            result = FT_ERR_INVALID_STATE;
        else
            result = FT_ERR_SUCCESS;
    }
    if (result == FT_ERR_SUCCESS)
    {
        this->_recipe_stations.insert(recipe_id, station_id);
        result = this->_recipe_stations.get_error();
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

struct game_crafting_query_stack
{
    int32_t slot;
    int32_t item_id;
    int32_t rarity;
    int32_t remaining;
};

static int32_t game_crafting_allocate_query(
    const ft_vector<game_crafting_ingredient> &ingredients, int32_t batch_count,
    std::vector<game_crafting_query_stack> &stacks, game_crafting_query_result &query,
    ft_vector<game_crafting_missing_ingredient> &missing)
{
    ft_vector<game_crafting_missing_ingredient> groups;
    int32_t result = groups.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    const ft_size_t ingredient_count = ingredients.size();
    ft_size_t index = 0U;
    while (index < ingredient_count)
    {
        const game_crafting_ingredient &ingredient = ingredients[index++];
        game_crafting_missing_ingredient group = {};
        group.item_id = ingredient.get_item_id();
        group.rarity = ingredient.get_rarity();
        group.alternative_count = ingredient.get_alternative_count();
        const int32_t ingredient_count_per_craft = ingredient.get_count();
        if (group.item_id <= 0 || ingredient_count_per_craft <= 0
            || group.rarity < -1 || group.alternative_count == 0U
            || group.alternative_count > game_crafting_ingredient::MAX_ALTERNATIVE_ITEM_IDS)
            return (FT_ERR_CONFIGURATION);
        if (ingredient_count_per_craft > INT32_MAX / batch_count)
            return (FT_ERR_OUT_OF_RANGE);
        group.required_units = ingredient_count_per_craft * batch_count;
        uint32_t alternative_index = 0U;
        while (alternative_index < group.alternative_count)
        {
            const int32_t item_id = ingredient.get_alternative_item_id(alternative_index);
            if (item_id <= 0)
                return (FT_ERR_CONFIGURATION);
            group.alternative_item_ids[alternative_index++] = item_id;
        }
        result = groups.push_back(group);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }

    int32_t pass = 0;
    while (pass < 2)
    {
        auto *group = groups.begin();
        const auto *group_end = groups.end();
        while (group != group_end)
        {
            const ft_bool wildcard = group->rarity == -1;
            const ft_bool wildcard_pass = pass == 1;
            if (wildcard == wildcard_pass)
            {
                int32_t remaining = group->required_units;
                const int32_t *alternatives = group->alternative_item_ids;
                const int32_t *alternatives_end = alternatives + group->alternative_count;
                auto stack = stacks.begin();
                while (stack != stacks.end() && remaining > 0)
                {
                    if (stack->remaining > 0 && (wildcard == FT_TRUE || stack->rarity == group->rarity)
                        && std::find(alternatives, alternatives_end, stack->item_id) != alternatives_end)
                    {
                        const int32_t consumed = std::min(stack->remaining, remaining);
                        stack->remaining -= consumed;
                        remaining -= consumed;
                    }
                    ++stack;
                }
                group->available_units = group->required_units - remaining;
                group->missing_units = remaining;
            }
            ++group;
        }
        ++pass;
    }
    const auto *group = groups.begin();
    const auto *group_end = groups.end();
    while (group != group_end)
    {
        if (group->missing_units > 0)
        {
            result = missing.push_back(*group);
            if (result != FT_ERR_SUCCESS)
                return (result);
            ++query.missing_group_count;
            if (query.missing_units > INT32_MAX - group->missing_units)
                query.missing_units = INT32_MAX;
            else
                query.missing_units += group->missing_units;
        }
        ++group;
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::query_recipe_at_station(int32_t recipe_id,
    const game_inventory &inventory, int32_t station_id,
    game_crafting_query_result *query_result,
    ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    std::vector<game_crafting_query_stack> stacks;
    ft_bool crafting_locked;
    ft_bool inventory_locked;
    int32_t result;

    if (query_result == ft_nullptr || missing_groups == ft_nullptr
        || station_id < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    query_result->found = FT_FALSE;
    query_result->known = FT_FALSE;
    query_result->craftable = FT_FALSE;
    query_result->station_available = FT_FALSE;
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
    const Pair<int32_t, int32_t> *required_station =
        this->_recipe_stations.find(recipe_id);
    if (required_station == this->_recipe_stations.end()
        || required_station->value == station_id)
        query_result->station_available = FT_TRUE;
    else
    {
        this->unlock_internal(crafting_locked);
        return (FT_ERR_SUCCESS);
    }
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(crafting_locked);
        query_result->found = FT_FALSE;
        query_result->known = FT_FALSE;
        return (result);
    }
    if (result == FT_ERR_SUCCESS && inventory._next_slot < 0)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
    {
        const ft_size_t occupied = inventory._items.size();
        result = inventory._items.get_error();
        if (result == FT_ERR_SUCCESS)
        {
            try
            {
                if (occupied > stacks.max_size())
                    result = FT_ERR_OUT_OF_RANGE;
                else
                    stacks.reserve(static_cast<std::size_t>(occupied));
            }
            catch (const std::bad_alloc &)
            {
                result = FT_ERR_NO_MEMORY;
            }
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        using item_entry = Pair<int32_t, ft_sharedptr<game_item>>;
        result = inventory._items.visit_entries([&](const item_entry &entry) noexcept -> int32_t
        {
            if (entry.key < 0 || entry.key >= inventory._next_slot)
                return (FT_ERR_INVALID_STATE);
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
            const game_crafting_query_stack snapshot = {entry.key, item->_item_id, item->_rarity, item->_stack_size};
            if (item_locked == FT_TRUE)
                item->unlock_internal(item_locked);
            if (snapshot.remaining < 0)
                return (FT_ERR_INVALID_STATE);
            if (snapshot.remaining == 0)
                return (FT_ERR_SUCCESS);
            try
            {
                stacks.push_back(snapshot);
            }
            catch (const std::bad_alloc &)
            {
                return (FT_ERR_NO_MEMORY);
            }
            catch (const std::length_error &)
            {
                return (FT_ERR_OUT_OF_RANGE);
            }
            return (FT_ERR_SUCCESS);
        });
    }
    inventory.unlock(inventory_locked);
    if (result == FT_ERR_SUCCESS && stacks.size() > 1U)
    {
        const auto in_slot_order = [](const game_crafting_query_stack &left, const game_crafting_query_stack &right)
        {
            return (left.slot < right.slot);
        };
        const auto begin = stacks.begin();
        const auto end = stacks.end();
        if (!std::is_sorted(begin, end, in_slot_order))
            std::sort(begin, end, in_slot_order);
    }
    if (result == FT_ERR_SUCCESS)
        result = game_crafting_allocate_query(recipe_entry->value, 1, stacks, *query_result, *missing_groups);
    this->unlock_internal(crafting_locked);
    if (result != FT_ERR_SUCCESS)
    {
        query_result->found = FT_FALSE;
        query_result->known = FT_FALSE;
        query_result->missing_group_count = 0U;
        query_result->missing_units = 0;
        query_result->station_available = FT_FALSE;
        missing_groups->clear();
        return (result);
    }
    query_result->craftable = (query_result->known == FT_TRUE
        && query_result->missing_group_count == 0U) ? FT_TRUE : FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::query_recipe_at_station_snapshot(int32_t recipe_id,
    const game_inventory_snapshot &inventory_snapshot, int32_t station_id,
    game_crafting_query_result *query_result,
    ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept
{
    return (this->query_recipe_at_station_snapshot(recipe_id,
        inventory_snapshot, station_id, 1, query_result, missing_groups));
}

int32_t game_crafting::query_recipe_at_station_snapshot(int32_t recipe_id,
    const game_inventory_snapshot &inventory_snapshot, int32_t station_id,
    int32_t batch_count, game_crafting_query_result *query_result,
    ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    const Pair<int32_t, int32_t> *required_station;
    std::vector<game_crafting_query_stack> stacks;
    int32_t result;
    ft_bool crafting_locked = FT_FALSE;

    if (query_result == ft_nullptr || missing_groups == ft_nullptr
        || station_id < 0 || batch_count <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    query_result->found = FT_FALSE;
    query_result->known = FT_FALSE;
    query_result->craftable = FT_FALSE;
    query_result->station_available = FT_FALSE;
    query_result->missing_group_count = 0U;
    query_result->missing_units = 0;
    if (missing_groups->is_initialised() == FT_CLASS_STATE_INITIALISED)
        missing_groups->clear();
    else
    {
        result = missing_groups->initialize();
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
    }
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::query_recipe_at_station_snapshot");
    if (inventory_snapshot.weight_limit < 0
        || inventory_snapshot.current_weight < 0
        || inventory_snapshot.next_slot < 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    uint64_t calculated_slots = 0U;
    int64_t calculated_weight = 0;
    std::size_t physical_count = 0U;
    try
    {
        stacks.reserve(inventory_snapshot.items.size());
        auto item = inventory_snapshot.items.begin();
        while (item != inventory_snapshot.items.end())
        {
            if (item->slot < 0 || item->slot >= inventory_snapshot.next_slot
                || item->item_id <= 0 || item->stack_size <= 0
                || item->max_stack <= 0 || item->stack_size > item->max_stack
                || item->width <= 0 || item->height <= 0
                || (item->instance_id != 0U
                    && (item->stack_size != 1 || item->max_stack != 1)))
                return (this->set_error(FT_ERR_INVALID_ARGUMENT));
            if (calculated_weight > INT32_MAX - item->stack_size)
                return (this->set_error(FT_ERR_INVALID_ARGUMENT));
            calculated_weight += item->stack_size;
            const uint64_t item_slots = static_cast<uint64_t>(item->width) * item->height;
            if (calculated_slots > UINT64_MAX - item_slots)
                return (this->set_error(FT_ERR_INVALID_ARGUMENT));
            calculated_slots += item_slots;
            stacks.push_back({item->slot, item->item_id, item->rarity, item->stack_size});
            if (item->instance_id != 0U)
                ++physical_count;
            ++item;
        }
        if (calculated_weight != inventory_snapshot.current_weight
#if USE_INVENTORY_SLOTS
            || calculated_slots != inventory_snapshot.used_slots
            || (inventory_snapshot.capacity != 0U && calculated_slots > inventory_snapshot.capacity)
#endif
            || (inventory_snapshot.weight_limit != 0 && calculated_weight > inventory_snapshot.weight_limit))
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        const auto same_slot = [](const game_crafting_query_stack &left,
                                 const game_crafting_query_stack &right)
        {
            return (left.slot == right.slot);
        };
        const auto in_slot_order = [](const game_crafting_query_stack &left,
                                     const game_crafting_query_stack &right)
        {
            return (left.slot < right.slot);
        };
        if (!std::is_sorted(stacks.begin(), stacks.end(), in_slot_order))
            std::sort(stacks.begin(), stacks.end(), in_slot_order);
        if (std::adjacent_find(stacks.begin(), stacks.end(), same_slot) != stacks.end())
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));

        if (physical_count > 1U)
        {
            std::vector<uint64_t> instance_ids;
            instance_ids.reserve(physical_count);
            item = inventory_snapshot.items.begin();
            while (item != inventory_snapshot.items.end())
            {
                if (item->instance_id != 0U)
                    instance_ids.push_back(item->instance_id);
                ++item;
            }
            if (!std::is_sorted(instance_ids.begin(), instance_ids.end()))
                std::sort(instance_ids.begin(), instance_ids.end());
            if (std::adjacent_find(instance_ids.begin(), instance_ids.end()) != instance_ids.end())
                return (this->set_error(FT_ERR_ALREADY_EXISTS));
        }
    }
    catch (const std::bad_alloc &)
    {
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    catch (const std::length_error &)
    {
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    result = this->lock_internal(&crafting_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    recipe_entry = this->_recipes.find(recipe_id);
    if (recipe_entry == this->_recipes.end())
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_NOT_FOUND));
    }
    query_result->found = FT_TRUE;
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    query_result->known = (unlock_entry != this->_unlocked_recipes.end()
        && unlock_entry->value == FT_TRUE) ? FT_TRUE : FT_FALSE;
    required_station = this->_recipe_stations.find(recipe_id);
    if (required_station == this->_recipe_stations.end()
        || required_station->value == station_id)
        query_result->station_available = FT_TRUE;
    else
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_SUCCESS));
    }
    result = game_crafting_allocate_query(recipe_entry->value, batch_count,
        stacks, *query_result, *missing_groups);
    this->unlock_internal(crafting_locked);
    if (result != FT_ERR_SUCCESS)
    {
        query_result->found = FT_FALSE;
        query_result->known = FT_FALSE;
        query_result->craftable = FT_FALSE;
        query_result->station_available = FT_FALSE;
        query_result->missing_group_count = 0U;
        query_result->missing_units = 0;
        missing_groups->clear();
        return (this->set_error(result));
    }
    query_result->craftable = (query_result->known == FT_TRUE
        && query_result->missing_group_count == 0U) ? FT_TRUE : FT_FALSE;
    return (this->set_error(FT_ERR_SUCCESS));
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

static ft_bool game_crafting_snapshots_equal(const game_inventory_item_snapshot &left,
    const game_inventory_item_snapshot &right) noexcept
{
    return (left.slot == right.slot && left.item_id == right.item_id
        && left.instance_id == right.instance_id && left.stack_size == right.stack_size
        && left.max_stack == right.max_stack && left.rarity == right.rarity
        && left.width == right.width && left.height == right.height
        && left.modifier1_id == right.modifier1_id && left.modifier1_value == right.modifier1_value
        && left.modifier2_id == right.modifier2_id && left.modifier2_value == right.modifier2_value
        && left.modifier3_id == right.modifier3_id && left.modifier3_value == right.modifier3_value
        && left.modifier4_id == right.modifier4_id && left.modifier4_value == right.modifier4_value);
}

int32_t game_crafting::prepare_craft(const game_inventory &inventory, int32_t recipe_id,
    int32_t batch_count, game_crafting_prepared *prepared) noexcept
{
    return (this->prepare_craft_at_station(inventory, recipe_id, 0, batch_count, prepared));
}

int32_t game_crafting::prepare_craft_at_station(const game_inventory &inventory,
    int32_t recipe_id, int32_t station_id, int32_t batch_count,
    game_crafting_prepared *prepared) noexcept
{
    return (this->prepare_craft_internal(inventory, recipe_id, station_id,
        batch_count, prepared, FT_TRUE, ft_nullptr));
}

static int32_t game_crafting_scale_output(const game_crafting_output_snapshot &definition,
    int32_t batch_count, game_crafting_output_snapshot &snapshot) noexcept
{
    if (definition.primary.quantity > INT32_MAX / batch_count
        || definition.byproduct_count > GAME_CRAFTING_MAX_BYPRODUCTS)
        return (FT_ERR_INVALID_ARGUMENT);
    uint32_t index = 0U;
    while (index < definition.byproduct_count)
    {
        if (definition.byproducts[index].quantity > INT32_MAX / batch_count)
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    snapshot = definition;
    snapshot.primary.quantity *= batch_count;
    index = 0U;
    while (index < snapshot.byproduct_count)
        snapshot.byproducts[index++].quantity *= batch_count;
    return (FT_ERR_SUCCESS);
}

int32_t game_crafting::query_recipe_output_snapshot(int32_t recipe_id,
    int32_t batch_count, game_crafting_output_snapshot *output_out) const noexcept
{
    if (output_out == ft_nullptr || batch_count <= 0)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_crafting::query_recipe_output_snapshot");
    ft_bool crafting_locked = FT_FALSE;
    int32_t result = this->lock_internal(&crafting_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    const auto *entry = this->_recipe_outputs.find(recipe_id);
    if (entry == this->_recipe_outputs.end())
        result = FT_ERR_NOT_FOUND;
    else
        result = game_crafting_scale_output(entry->value, batch_count, *output_out);
    this->unlock_internal(crafting_locked);
    return (this->set_error(result));
}

int32_t game_crafting::stage_product(game_inventory &staged,
    const game_crafting_product_snapshot &product) noexcept
{
    ft_sharedptr<game_item> item;
    int32_t result = item.initialize(new (std::nothrow) game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (!item)
        return (FT_ERR_NO_MEMORY);
    result = item->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    item->set_item_id(product.item_id);
    item->set_max_stack(product.max_stack);
    item->set_stack_size(product.quantity);
    item->set_rarity(product.rarity);
    item->set_width(product.width);
    item->set_height(product.height);
    item->set_modifier1_id(product.modifier1_id);
    item->set_modifier1_value(product.modifier1_value);
    item->set_modifier2_id(product.modifier2_id);
    item->set_modifier2_value(product.modifier2_value);
    item->set_modifier3_id(product.modifier3_id);
    item->set_modifier3_value(product.modifier3_value);
    item->set_modifier4_id(product.modifier4_id);
    item->set_modifier4_value(product.modifier4_value);
    return (staged.add_item_in_place(item));
}

int32_t game_crafting::prepare_processing_at_station(
    const game_inventory &inventory, int32_t recipe_id, int32_t station_id,
    int32_t batch_count, game_crafting_prepared *prepared,
    game_crafting_output_snapshot *output_out) noexcept
{
    if (output_out == ft_nullptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    return (this->prepare_craft_internal(inventory, recipe_id, station_id,
        batch_count, prepared, FT_FALSE, output_out));
}

int32_t game_crafting::consume_ingredients(game_inventory &staged,
    const ft_vector<game_crafting_ingredient> &ingredients, int32_t batch_count,
    ft_bool prepared_order) noexcept
{
    using item_entry = Pair<int32_t, ft_sharedptr<game_item>>;
    struct debit_entry
    {
        item_entry *entry;
        ft_size_t storage_index;
        ft_bool exhausted;
    };

    const ft_size_t item_count = staged._items.size();
    ft_vector<debit_entry> slots;
    int32_t result = slots.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    slots.reserve(item_count);
    if (slots.get_error() != FT_ERR_SUCCESS)
        return (slots.get_error());
    item_entry *end = staged._items.end();
    item_entry *entry = end;
    if (item_count != 0U)
        entry -= item_count;
    ft_bool ascending = FT_TRUE;
    int32_t previous_slot = -1;
    ft_size_t storage_index = 0U;
    while (entry != end)
    {
        if (entry->key >= 0 && entry->key < staged._next_slot && entry->value)
        {
            if (prepared_order == FT_TRUE && entry->key < previous_slot)
                ascending = FT_FALSE;
            previous_slot = entry->key;
            result = slots.push_back({entry, storage_index, FT_FALSE});
            if (result != FT_ERR_SUCCESS)
                return (result);
        }
        ++entry;
        ++storage_index;
    }
    if (ascending == FT_FALSE)
    {
        std::sort(slots.begin(), slots.end(), [](const debit_entry &left, const debit_entry &right)
        {
            return (left.entry->key < right.entry->key);
        });
    }
    ft_bool removed_any = FT_FALSE;
    const ft_size_t ingredient_count = ingredients.size();
    const ft_size_t slot_count = slots.size();
    int32_t pass = 0;
    const int32_t pass_count = 1 + static_cast<int32_t>(prepared_order);
    while (pass < pass_count)
    {
        ft_size_t ingredient_index = 0U;
        while (ingredient_index < ingredient_count)
        {
            const game_crafting_ingredient &ingredient = ingredients[ingredient_index++];
            const int32_t rarity = ingredient.get_rarity();
            if (prepared_order == FT_TRUE
                && ((pass == 0 && rarity == -1) || (pass == 1 && rarity != -1)))
                continue ;
            const int32_t per_craft = ingredient.get_count();
            if (per_craft < 0 || per_craft > FT_INT32_MAX / batch_count)
                return (FT_ERR_INVALID_ARGUMENT);
            if (prepared_order == FT_TRUE)
            {
                const uint32_t alternative_count = ingredient.get_alternative_count();
                if (per_craft == 0 || alternative_count == 0U
                    || alternative_count > game_crafting_ingredient::MAX_ALTERNATIVE_ITEM_IDS)
                    return (FT_ERR_INVALID_ARGUMENT);
            }

            int32_t remaining = per_craft * batch_count;
            int32_t primary_item_id = 0;
            if (prepared_order == FT_FALSE)
                primary_item_id = ingredient.get_item_id();
            ft_size_t cursor = 0U;
            while (cursor < slot_count && remaining > 0)
            {
                debit_entry &debit = slots[cursor++];
                if (debit.exhausted == FT_TRUE)
                    continue ;
                game_item *item = debit.entry->value.get();
                const int32_t item_id = item->get_item_id();
                if ((prepared_order == FT_TRUE
                        && ingredient.matches_item_id(item_id) == FT_FALSE)
                    || (prepared_order == FT_FALSE && primary_item_id != item_id)
                    || (rarity != -1 && item->get_rarity() != rarity))
                    continue ;
                const int32_t stack_size = item->get_stack_size();
                int32_t consumed = stack_size;
                if (consumed > remaining)
                    consumed = remaining;
                const int64_t weight = static_cast<int64_t>(staged._current_weight) - consumed;
                const int64_t pending = static_cast<int64_t>(remaining) - consumed;
                if (weight < INT32_MIN || weight > INT32_MAX || pending > INT32_MAX)
                    return (FT_ERR_OUT_OF_RANGE);
                item->sub_from_stack(consumed);
                result = item->get_error();
                if (result != FT_ERR_SUCCESS)
                    return (result);
                staged._current_weight = static_cast<int32_t>(weight);
                remaining = static_cast<int32_t>(pending);
                if (consumed == stack_size)
                {
                    debit.exhausted = FT_TRUE;
                    removed_any = FT_TRUE;
#if USE_INVENTORY_SLOTS
                    const int64_t area = static_cast<int64_t>(item->get_width()) * item->get_height();
                    if (area >= 0 && static_cast<uint64_t>(area) <= staged._used_slots)
                        staged._used_slots -= static_cast<ft_size_t>(area);
#endif
                }
            }
            if (remaining > 0)
                return (FT_ERR_GAME_GENERAL_ERROR);
        }
        ++pass;
    }
    if (removed_any == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (ascending == FT_FALSE)
    {
        std::sort(slots.begin(), slots.end(), [](const debit_entry &left, const debit_entry &right)
        {
            return (left.storage_index < right.storage_index);
        });
    }
    const auto *next = slots.begin();
    const auto *slot_end = slots.end();
    return (staged._items.erase_if(
        [&next, slot_end](const item_entry &, ft_size_t index) noexcept -> ft_bool
        {
            if (next == slot_end || next->storage_index != index)
                return (FT_FALSE);
            const ft_bool exhausted = next->exhausted;
            ++next;
            return (exhausted);
        },
        [](item_entry &target, item_entry &source) noexcept
        {
            target.key = source.key;
            target.value.swap(source.value);
            return ;
        }));
}

int32_t game_crafting::prepare_craft_internal(const game_inventory &inventory,
    int32_t recipe_id, int32_t station_id, int32_t batch_count,
    game_crafting_prepared *prepared, ft_bool include_outputs,
    game_crafting_output_snapshot *output_out) noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    game_inventory staged;
    game_crafting_prepared candidate;
    ft_size_t item_count;
    ft_size_t byproduct_index;
    ft_bool crafting_locked;
    ft_bool inventory_locked;
    game_crafting_output_snapshot output_snapshot = {};
    int32_t result;

    if (prepared == ft_nullptr || batch_count <= 0 || station_id < 0
        || (include_outputs == FT_FALSE && output_out == ft_nullptr))
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
    const Pair<int32_t, int32_t> *required_station =
        this->_recipe_stations.find(recipe_id);
    if (result == FT_ERR_SUCCESS
        && required_station != this->_recipe_stations.end()
        && required_station->value != station_id)
        result = FT_ERR_PERMISSION_DENIED;
    unlock_entry = this->_unlocked_recipes.find(recipe_id);
    if (result == FT_ERR_SUCCESS
        && (unlock_entry == this->_unlocked_recipes.end()
            || unlock_entry->value != FT_TRUE))
        result = FT_ERR_INVALID_STATE;
    const auto *output_entry = this->_recipe_outputs.find(recipe_id);
    if (result == FT_ERR_SUCCESS)
    {
        if (output_entry == this->_recipe_outputs.end())
            result = FT_ERR_NOT_FOUND;
        else
            result = game_crafting_scale_output(output_entry->value, batch_count, output_snapshot);
    }
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
    item_count = inventory._items.size();
    result = inventory._items.get_error();
    if (result == FT_ERR_SUCCESS)
    {
        candidate._expected_items.reserve(item_count);
        result = candidate._expected_items.get_error();
    }
    if (result == FT_ERR_SUCCESS)
    {
        using item_entry = Pair<int32_t, ft_sharedptr<game_item>>;
        result = inventory._items.visit_entries([&](const item_entry &entry) noexcept -> int32_t
        {
            if (!entry.value)
                return (FT_ERR_SUCCESS);
            ft_sharedptr<game_item> staged_copy;
            int32_t capture_error = game_crafting_clone_item(entry.value, &staged_copy);
            game_inventory_item_snapshot snapshot;
            if (capture_error == FT_ERR_SUCCESS)
                capture_error = staged_copy->capture_inventory_fields(entry.key, snapshot);
            if (capture_error == FT_ERR_SUCCESS)
                capture_error = candidate._expected_items.push_back(snapshot);
            if (capture_error != FT_ERR_SUCCESS)
                return (capture_error);
            staged._items.insert(entry.key, ft_move(staged_copy));
            return (staged._items.get_error());
        });
    }
    if (result == FT_ERR_SUCCESS && candidate._expected_items.size() > 1U)
    {
        const auto in_slot_order = [](const game_inventory_item_snapshot &left,
                                     const game_inventory_item_snapshot &right)
        {
            return (left.slot < right.slot);
        };
        auto *begin = candidate._expected_items.begin();
        auto *end = candidate._expected_items.end();
        if (!std::is_sorted(begin, end, in_slot_order))
            std::sort(begin, end, in_slot_order);
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
        candidate._expected_revision = inventory._revision;
        candidate._result_capacity = inventory._capacity;
        candidate._result_weight_limit = inventory._weight_limit;
        candidate._result_used_slots = inventory._used_slots;
        candidate._result_current_weight = inventory._current_weight;
        candidate._result_next_slot = inventory._next_slot;
    }
    if (result == FT_ERR_SUCCESS)
        result = consume_ingredients(staged, recipe_entry->value, batch_count, FT_TRUE);
    if (result == FT_ERR_SUCCESS && include_outputs == FT_TRUE)
    {
        candidate._primary_output_slot = staged._next_slot;
        candidate._primary_output_item_id = output_snapshot.primary.item_id;
        candidate._primary_output_quantity = output_snapshot.primary.quantity;
        result = stage_product(staged, output_snapshot.primary);
        byproduct_index = 0U;
        while (result == FT_ERR_SUCCESS && byproduct_index < output_snapshot.byproduct_count)
            result = stage_product(staged, output_snapshot.byproducts[byproduct_index++]);
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
        result = prepared->_expected_items.move(candidate._expected_items);
        if (result == FT_ERR_SUCCESS)
            result = prepared->_result_items.move_from(candidate._result_items);
        if (result == FT_ERR_SUCCESS)
        {
            prepared->_expected_capacity = candidate._expected_capacity;
            prepared->_expected_used_slots = candidate._expected_used_slots;
            prepared->_expected_weight_limit = candidate._expected_weight_limit;
            prepared->_expected_current_weight = candidate._expected_current_weight;
            prepared->_expected_next_slot = candidate._expected_next_slot;
            prepared->_expected_revision = candidate._expected_revision;
            prepared->_result_capacity = candidate._result_capacity;
            prepared->_result_used_slots = candidate._result_used_slots;
            prepared->_result_weight_limit = candidate._result_weight_limit;
            prepared->_result_current_weight = candidate._result_current_weight;
            prepared->_result_next_slot = candidate._result_next_slot;
            prepared->_primary_output_slot = candidate._primary_output_slot;
            prepared->_primary_output_item_id =
                candidate._primary_output_item_id;
            prepared->_primary_output_quantity =
                candidate._primary_output_quantity;
            prepared->_ready = FT_TRUE;
        }
    }
    if (result == FT_ERR_SUCCESS && output_out != ft_nullptr)
        *output_out = output_snapshot;
    inventory.unlock(inventory_locked);
    this->unlock_internal(crafting_locked);
    return (this->set_error(result));
}

int32_t game_crafting::commit_craft(game_inventory &inventory,
    game_crafting_prepared *prepared) noexcept
{
    return (this->commit_craft(inventory, prepared, 0U));
}

int32_t game_crafting::commit_processing_inputs(game_inventory &inventory,
    game_crafting_prepared *prepared) noexcept
{
    return (this->commit_craft(inventory, prepared, 0U));
}

int32_t game_crafting::commit_craft(game_inventory &inventory,
    game_crafting_prepared *prepared, uint64_t output_instance_id) noexcept
{
    Pair<int32_t, ft_sharedptr<game_item>> *primary_output;
    Pair<int32_t, ft_sharedptr<game_item>> *existing;
    game_item *assigned_output = ft_nullptr;
    ft_bool inventory_locked = FT_FALSE;
    ft_bool map_locked = FT_FALSE;
    int32_t result;

    if (prepared == ft_nullptr
        || prepared->_initialised_state != FT_CLASS_STATE_INITIALISED
        || prepared->_ready != FT_TRUE)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (inventory._capacity != prepared->_expected_capacity
        || inventory._used_slots != prepared->_expected_used_slots
        || inventory._weight_limit != prepared->_expected_weight_limit
        || inventory._current_weight != prepared->_expected_current_weight
        || inventory._next_slot != prepared->_expected_next_slot
        || inventory._revision != prepared->_expected_revision)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
        result = inventory._items.lock(&map_locked);
    const ft_size_t expected_count = prepared->_expected_items.size();
    if (result == FT_ERR_SUCCESS && inventory._items.size() != expected_count)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS
        && inventory._revision == std::numeric_limits<uint64_t>::max())
        result = FT_ERR_OUT_OF_RANGE;
    const auto *expected_begin = prepared->_expected_items.begin();
    const auto *expected_end = prepared->_expected_items.end();
    if (result == FT_ERR_SUCCESS && expected_count != 0U)
    {
        const auto *actual_end = inventory._items.end();
        const auto *actual = actual_end - expected_count;
        const auto *in_order = expected_begin;
        while (actual != actual_end && result == FT_ERR_SUCCESS)
        {
            const auto *expected = in_order++;
            if (expected->slot != actual->key)
            {
                expected = std::lower_bound(expected_begin, expected_end, actual->key,
                    [](const game_inventory_item_snapshot &item, int32_t slot)
                    {
                        return (item.slot < slot);
                    });
            }
            if (expected == expected_end || expected->slot != actual->key || !actual->value)
                result = FT_ERR_INVALID_STATE;
            else
            {
                game_inventory_item_snapshot snapshot;
                result = actual->value->capture_inventory_fields(actual->key, snapshot);
                if (result == FT_ERR_SUCCESS && !game_crafting_snapshots_equal(snapshot, *expected))
                    result = FT_ERR_INVALID_STATE;
            }
            ++actual;
        }
    }
    if (result == FT_ERR_SUCCESS && output_instance_id != 0U)
    {
        primary_output = prepared->_result_items.find(
            prepared->_primary_output_slot);
        if (primary_output == prepared->_result_items.end()
            || !primary_output->value
            || primary_output->value->get_item_id()
                != prepared->_primary_output_item_id
            || prepared->_primary_output_quantity != 1
            || primary_output->value->get_stack_size() != 1
            || primary_output->value->get_instance_id() != 0U)
            result = FT_ERR_INVALID_ARGUMENT;
        const auto *expected = expected_begin;
        while (result == FT_ERR_SUCCESS && expected != expected_end)
        {
            if (expected->instance_id == output_instance_id)
                result = FT_ERR_ALREADY_EXISTS;
            ++expected;
        }
        ft_size_t index = 0U;
        const ft_size_t count = prepared->_result_items.size();
        existing = prepared->_result_items.end() - count;
        while (result == FT_ERR_SUCCESS && index < count)
        {
            if (existing->key != prepared->_primary_output_slot
                && existing->value
                && existing->value->get_instance_id() == output_instance_id)
                result = FT_ERR_ALREADY_EXISTS;
            existing++;
            index++;
        }
        if (result == FT_ERR_SUCCESS)
        {
            primary_output->value->set_instance_id(output_instance_id);
            result = primary_output->value->get_error();
            if (result == FT_ERR_SUCCESS)
                assigned_output = primary_output->value.get();
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = inventory._items.move_from(prepared->_result_items);
    if (result == FT_ERR_SUCCESS)
    {
        inventory._revision += 1U;
        inventory._capacity = prepared->_result_capacity;
        inventory._used_slots = prepared->_result_used_slots;
        inventory._weight_limit = prepared->_result_weight_limit;
        inventory._current_weight = prepared->_result_current_weight;
        inventory._next_slot = prepared->_result_next_slot;
        prepared->_ready = FT_FALSE;
    }
    else if (assigned_output != ft_nullptr)
        assigned_output->set_instance_id(0U);
    if (map_locked == FT_TRUE)
        (void)inventory._items.unlock(map_locked);
    inventory.unlock(inventory_locked);
    return (this->set_error(result));
}

int32_t game_crafting::craft_item(game_inventory &inventory, int32_t recipe_id,
    const ft_sharedptr<game_item> &output) noexcept
{
    if (!output)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_crafting::craft_item");
    ft_bool crafting_locked = FT_FALSE;
    int32_t result = this->lock_internal(&crafting_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    const auto *recipe = this->_recipes.find(recipe_id);
    if (recipe == this->_recipes.end())
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_NOT_FOUND));
    }
    const auto *unlocked = this->_unlocked_recipes.find(recipe_id);
    if (unlocked == this->_unlocked_recipes.end() || unlocked->value != FT_TRUE)
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_recipe_stations.find(recipe_id) != this->_recipe_stations.end())
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_PERMISSION_DENIED));
    }
    if (this->_recipe_outputs.find(recipe_id) != this->_recipe_outputs.end())
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(FT_ERR_INVALID_STATE));
    }

    ft_bool inventory_locked = FT_FALSE;
    result = inventory.lock(&inventory_locked);
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(crafting_locked);
        return (this->set_error(result));
    }
    game_inventory staged;
    if (inventory._revision == std::numeric_limits<uint64_t>::max())
        result = FT_ERR_OUT_OF_RANGE;
    if (result == FT_ERR_SUCCESS)
        result = staged.initialize(inventory._capacity, inventory._weight_limit);
    if (result == FT_ERR_SUCCESS)
        result = staged._items.copy_from(inventory._items);
    if (result == FT_ERR_SUCCESS)
    {
        staged._used_slots = inventory._used_slots;
        staged._current_weight = inventory._current_weight;
        staged._next_slot = inventory._next_slot;
        const ft_size_t item_count = staged._items.size();
        auto *end = staged._items.end();
        auto *entry = end;
        if (item_count != 0U)
            entry -= item_count;
        while (result == FT_ERR_SUCCESS && entry != end)
        {
            if (entry->value)
            {
                ft_sharedptr<game_item> copy;
                result = game_crafting_clone_item(entry->value, &copy);
                if (result == FT_ERR_SUCCESS)
                    result = entry->value.move(copy);
            }
            ++entry;
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = consume_ingredients(staged, recipe->value, 1, FT_FALSE);
    if (result == FT_ERR_SUCCESS)
        result = staged.add_item_in_place(output);
    if (result == FT_ERR_SUCCESS)
        result = inventory._items.move_from(staged._items);
    if (result == FT_ERR_SUCCESS)
    {
        inventory._used_slots = staged._used_slots;
        inventory._current_weight = staged._current_weight;
        inventory._next_slot = staged._next_slot;
        result = inventory.bump_revision();
    }
    inventory.unlock(inventory_locked);
    this->unlock_internal(crafting_locked);
    return (this->set_error(result));
}
