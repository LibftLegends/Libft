#include "../Printf/printf.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "game_crafting.hpp"
#include <algorithm>
#include <new>
#include <limits>
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
    ft_sharedptr<game_item> owned_definition;
    game_crafting_output output = {};
    ft_size_t byproduct_index;
    ft_bool lock_acquired;
    int32_t result;

    if (!output_definition || output_quantity <= 0
        || output_definition->get_instance_id() != 0U
        || output_definition->get_max_stack() <= 0
        || byproducts.size() > GAME_CRAFTING_MAX_BYPRODUCTS
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
    output.item_definition = owned_definition;
    output.quantity = output_quantity;
    output.byproduct_count = byproducts.size();
    byproduct_index = 0U;
    while (byproduct_index < byproducts.size())
    {
        const game_crafting_byproduct &source = byproducts[byproduct_index];

        if (source.item_id <= 0 || source.quantity <= 0
            || source.max_stack <= 0 || source.instance_id != 0U
            || source.width <= 0 || source.height <= 0)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        output.byproducts[byproduct_index] = source;
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
    this->_recipe_outputs.insert(recipe_id,
        static_cast<game_crafting_output &&>(output));
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

int32_t game_crafting::query_recipe_at_station(int32_t recipe_id,
    const game_inventory &inventory, int32_t station_id,
    game_crafting_query_result *query_result,
    ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept
{
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    const Pair<int32_t, ft_sharedptr<game_item>> *item_entry;
    ft_vector<game_crafting_missing_ingredient> all_groups;
    ft_size_t index;
    ft_size_t item_index;
    ft_size_t item_count;
    int32_t remaining;
    std::vector<int32_t> unallocated_units;
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
    result = all_groups.initialize();
    if (result == FT_ERR_SUCCESS && inventory._next_slot < 0)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
    {
        try
        {
            unallocated_units.assign(
                static_cast<std::size_t>(inventory._next_slot), 0);
        }
        catch (const std::bad_alloc &)
        {
            result = FT_ERR_NO_MEMORY;
        }
    }
    item_count = inventory._items.size();
    item_entry = inventory._items.end() - item_count;
    item_index = 0U;
    while (result == FT_ERR_SUCCESS && item_index < item_count)
    {
        const int32_t slot = item_entry->key;

        if (slot < 0 || static_cast<std::size_t>(slot)
            >= unallocated_units.size())
            result = FT_ERR_INVALID_STATE;
        else if (item_entry->value)
            unallocated_units[static_cast<std::size_t>(slot)] =
                item_entry->value->get_stack_size();
        item_entry++;
        item_index++;
    }
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < recipe_entry->value.size())
    {
        game_crafting_missing_ingredient group = {};
        const game_crafting_ingredient &ingredient = recipe_entry->value[index];
        const uint32_t alternative_count = ingredient.get_alternative_count();

        group.item_id = ingredient.get_item_id();
        group.rarity = ingredient.get_rarity();
        group.alternative_count = alternative_count;
        group.required_units = ingredient.get_count();
        if (group.item_id <= 0 || group.required_units <= 0
            || group.rarity < -1 || alternative_count == 0U
            || alternative_count > game_crafting_ingredient::MAX_ALTERNATIVE_ITEM_IDS)
        {
            result = FT_ERR_CONFIGURATION;
            break ;
        }
        uint32_t alternative_index = 0U;
        while (alternative_index < alternative_count)
        {
            group.alternative_item_ids[alternative_index] =
                ingredient.get_alternative_item_id(alternative_index);
            if (group.alternative_item_ids[alternative_index] <= 0)
            {
                result = FT_ERR_CONFIGURATION;
                break ;
            }
            ++alternative_index;
        }
        if (result == FT_ERR_SUCCESS)
            result = all_groups.push_back(group);
        index++;
    }
    /* Allocate rarity-specific requirements before wildcard requirements.
     * Each inventory slot has one remaining-unit count shared by all groups. */
    for (int32_t pass = 0; pass < 2 && result == FT_ERR_SUCCESS; ++pass)
    {
        index = 0U;
        while (index < all_groups.size() && result == FT_ERR_SUCCESS)
        {
            game_crafting_missing_ingredient &group = all_groups[index];
            const game_crafting_ingredient &ingredient =
                recipe_entry->value[index];
            const ft_bool wildcard = group.rarity == -1
                ? FT_TRUE : FT_FALSE;

            if ((pass == 0 && wildcard == FT_TRUE)
                || (pass == 1 && wildcard == FT_FALSE))
            {
                ++index;
                continue;
            }
            remaining = group.required_units;
            item_entry = inventory._items.end() - item_count;
            item_index = 0U;
            while (item_index < item_count && remaining > 0)
            {
                const int32_t slot = item_entry->key;

                if (item_entry->value
                    && (wildcard == FT_TRUE
                        || item_entry->value->get_rarity() == group.rarity)
                    && ingredient.matches_item_id(
                        item_entry->value->get_item_id()) == FT_TRUE)
                {
                    const int32_t available_units =
                        unallocated_units[static_cast<std::size_t>(slot)];
                    const int32_t consumed = available_units < remaining
                        ? available_units : remaining;

                    unallocated_units[static_cast<std::size_t>(slot)]
                        -= consumed;
                    group.available_units += consumed;
                    remaining -= consumed;
                }
                ++item_entry;
                ++item_index;
            }
            group.missing_units = remaining;
            ++index;
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
    const Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    const Pair<int32_t, ft_bool> *unlock_entry;
    const Pair<int32_t, int32_t> *required_station;
    ft_vector<game_crafting_missing_ingredient> all_groups;
    std::vector<std::size_t> item_order;
    std::vector<int32_t> remaining_stack_sizes;
    std::vector<uint64_t> instance_ids;
    uint64_t calculated_slots = 0U;
    int64_t calculated_weight = 0;
    int32_t maximum_slot = -1;
    ft_size_t index;
    ft_size_t item_index;
    uint32_t alternative_index;
    int32_t remaining;
    int32_t result;
    ft_bool crafting_locked = FT_FALSE;

    if (query_result == ft_nullptr || missing_groups == ft_nullptr
        || station_id < 0)
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
    try
    {
        item_order.resize(inventory_snapshot.items.size());
        remaining_stack_sizes.resize(inventory_snapshot.items.size());
        instance_ids.reserve(inventory_snapshot.items.size());
    }
    catch (const std::bad_alloc &)
    {
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    index = 0U;
    while (index < inventory_snapshot.items.size())
    {
        item_order[index] = index;
        ++index;
    }
    std::sort(item_order.begin(), item_order.end(),
        [&inventory_snapshot](std::size_t left_index,
            std::size_t right_index)
        {
            return (inventory_snapshot.items[left_index].slot
                < inventory_snapshot.items[right_index].slot);
        });
    item_index = 0U;
    while (item_index < item_order.size())
    {
        const game_inventory_item_snapshot &item =
            inventory_snapshot.items[item_order[item_index]];
        uint64_t item_slots;

        if (item.slot < 0 || item.slot >= inventory_snapshot.next_slot
            || item.item_id <= 0 || item.stack_size <= 0
            || item.max_stack <= 0 || item.stack_size > item.max_stack
            || item.width <= 0 || item.height <= 0
            || (item.instance_id != 0U
                && (item.stack_size != 1 || item.max_stack != 1))
            || (item_index > 0U
                && inventory_snapshot.items[item_order[item_index - 1U]].slot
                    == item.slot))
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        if (calculated_weight > FT_INT32_MAX - item.stack_size)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        calculated_weight += item.stack_size;
        item_slots = static_cast<uint64_t>(item.width)
            * static_cast<uint64_t>(item.height);
        if (calculated_slots > UINT64_MAX - item_slots)
            return (this->set_error(FT_ERR_INVALID_ARGUMENT));
        calculated_slots += item_slots;
        if (item.slot > maximum_slot)
            maximum_slot = item.slot;
        remaining_stack_sizes[item_index] = item.stack_size;
        if (item.instance_id != 0U)
        {
            try
            {
                instance_ids.push_back(item.instance_id);
            }
            catch (const std::bad_alloc &)
            {
                return (this->set_error(FT_ERR_NO_MEMORY));
            }
        }
        ++item_index;
    }
    if (calculated_weight != inventory_snapshot.current_weight
#if USE_INVENTORY_SLOTS
        || calculated_slots != static_cast<uint64_t>(inventory_snapshot.used_slots)
        || (inventory_snapshot.capacity != 0U
            && calculated_slots > static_cast<uint64_t>(inventory_snapshot.capacity))
#endif
        || inventory_snapshot.next_slot <= maximum_slot
        || (inventory_snapshot.weight_limit != 0
            && calculated_weight > inventory_snapshot.weight_limit))
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    std::sort(instance_ids.begin(), instance_ids.end());
    index = 1U;
    while (index < instance_ids.size())
    {
        if (instance_ids[index] == instance_ids[index - 1U])
            return (this->set_error(FT_ERR_ALREADY_EXISTS));
        ++index;
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
    result = all_groups.initialize();
    index = 0U;
    while (result == FT_ERR_SUCCESS
        && index < recipe_entry->value.size())
    {
        game_crafting_missing_ingredient group = {};
        const game_crafting_ingredient &ingredient = recipe_entry->value[index];
        const uint32_t alternative_count = ingredient.get_alternative_count();

        group.item_id = ingredient.get_item_id();
        group.rarity = ingredient.get_rarity();
        group.alternative_count = alternative_count;
        group.required_units = ingredient.get_count();
        if (group.item_id <= 0 || group.required_units <= 0
            || group.rarity < -1 || alternative_count == 0U
            || alternative_count > game_crafting_ingredient::MAX_ALTERNATIVE_ITEM_IDS)
            result = FT_ERR_CONFIGURATION;
        alternative_index = 0U;
        while (result == FT_ERR_SUCCESS
            && alternative_index < alternative_count)
        {
            group.alternative_item_ids[alternative_index] =
                ingredient.get_alternative_item_id(alternative_index);
            if (group.alternative_item_ids[alternative_index] <= 0)
                result = FT_ERR_CONFIGURATION;
            ++alternative_index;
        }
        if (result == FT_ERR_SUCCESS)
            result = all_groups.push_back(group);
        ++index;
    }
    int32_t pass = 0;
    while (result == FT_ERR_SUCCESS && pass < 2)
    {
        index = 0U;
        while (result == FT_ERR_SUCCESS && index < all_groups.size())
        {
            game_crafting_missing_ingredient &group = all_groups[index];
            const game_crafting_ingredient &ingredient =
                recipe_entry->value[index];
            const ft_bool wildcard = group.rarity == -1
                ? FT_TRUE : FT_FALSE;

            if ((pass == 0 && wildcard == FT_TRUE)
                || (pass == 1 && wildcard == FT_FALSE))
            {
                ++index;
                continue ;
            }
            remaining = group.required_units;
            item_index = 0U;
            while (item_index < item_order.size() && remaining > 0)
            {
                const game_inventory_item_snapshot &item =
                    inventory_snapshot.items[item_order[item_index]];

                if ((wildcard == FT_TRUE || item.rarity == group.rarity)
                    && ingredient.matches_item_id(item.item_id) == FT_TRUE)
                {
                    const int32_t available_units =
                        remaining_stack_sizes[item_index];
                    const int32_t consumed = available_units < remaining
                        ? available_units : remaining;

                    remaining_stack_sizes[item_index] -= consumed;
                    group.available_units += consumed;
                    remaining -= consumed;
                }
                ++item_index;
            }
            group.missing_units = remaining;
            ++index;
        }
        ++pass;
    }
    index = 0U;
    while (result == FT_ERR_SUCCESS && index < all_groups.size())
    {
        const game_crafting_missing_ingredient &group = all_groups[index];

        if (group.missing_units > 0)
        {
            result = missing_groups->push_back(group);
            if (result == FT_ERR_SUCCESS)
            {
                ++query_result->missing_group_count;
                if (query_result->missing_units
                    > FT_INT32_MAX - group.missing_units)
                    query_result->missing_units = FT_INT32_MAX;
                else
                    query_result->missing_units += group.missing_units;
            }
        }
        ++index;
    }
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
        && left.get_modifier4_value() == right.get_modifier4_value()
        && left.get_instance_id() == right.get_instance_id());
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

int32_t game_crafting::prepare_craft_internal(const game_inventory &inventory,
    int32_t recipe_id, int32_t station_id, int32_t batch_count,
    game_crafting_prepared *prepared, ft_bool include_outputs,
    game_crafting_output_snapshot *output_out) noexcept
{
    Pair<int32_t, ft_vector<game_crafting_ingredient>> *recipe_entry;
    Pair<int32_t, ft_bool> *unlock_entry;
    Pair<int32_t, game_crafting_output> *output_entry;
    game_inventory staged;
    game_crafting_prepared candidate;
    ft_size_t index;
    ft_size_t item_count;
    ft_size_t byproduct_index;
    const Pair<int32_t, ft_sharedptr<game_item>> *entry;
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
    output_entry = this->_recipe_outputs.find(recipe_id);
    if (result == FT_ERR_SUCCESS && output_entry == this->_recipe_outputs.end())
        result = FT_ERR_NOT_FOUND;
    if (result == FT_ERR_SUCCESS
        && output_entry->value.quantity > FT_INT32_MAX / batch_count)
        result = FT_ERR_INVALID_ARGUMENT;
    byproduct_index = 0U;
    while (result == FT_ERR_SUCCESS
        && byproduct_index < output_entry->value.byproduct_count)
    {
        if (output_entry->value.byproducts[byproduct_index].quantity
            > FT_INT32_MAX / batch_count)
            result = FT_ERR_INVALID_ARGUMENT;
        byproduct_index++;
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
        candidate._expected_revision = inventory._revision;
        candidate._result_capacity = inventory._capacity;
        candidate._result_weight_limit = inventory._weight_limit;
        candidate._result_used_slots = inventory._used_slots;
        candidate._result_current_weight = inventory._current_weight;
        candidate._result_next_slot = inventory._next_slot;
    }
    for (int32_t pass = 0; pass < 2 && result == FT_ERR_SUCCESS; ++pass)
    {
        index = 0U;
        while (result == FT_ERR_SUCCESS
            && index < recipe_entry->value.size())
        {
            game_crafting_ingredient &ingredient =
                recipe_entry->value[index];
            const int32_t per_craft = ingredient.get_count();
            const int32_t ingredient_rarity = ingredient.get_rarity();
            int32_t remaining;
            int32_t slot;

            if ((pass == 0 && ingredient_rarity == -1)
                || (pass == 1 && ingredient_rarity != -1))
            {
                ++index;
                continue;
            }
            if (per_craft <= 0 || per_craft > FT_INT32_MAX / batch_count
                || ingredient.get_alternative_count() == 0U
                || ingredient.get_alternative_count()
                    > game_crafting_ingredient::MAX_ALTERNATIVE_ITEM_IDS)
            {
                result = FT_ERR_INVALID_ARGUMENT;
                break ;
            }
            remaining = per_craft * batch_count;
            slot = 0;
            while (slot < staged._next_slot && remaining > 0)
            {
                Pair<int32_t, ft_sharedptr<game_item>> *item_entry =
                    staged._items.find(slot);

                if (item_entry != staged._items.end() && item_entry->value
                    && ingredient.matches_item_id(
                        item_entry->value->get_item_id()) == FT_TRUE
                    && (ingredient_rarity == -1
                        || item_entry->value->get_rarity()
                            == ingredient_rarity))
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
                ++slot;
            }
            if (remaining > 0)
                result = FT_ERR_GAME_GENERAL_ERROR;
            ++index;
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        const ft_sharedptr<game_item> &definition =
            output_entry->value.item_definition;
        output_snapshot.primary.item_id = definition->get_item_id();
        output_snapshot.primary.quantity =
            output_entry->value.quantity * batch_count;
        output_snapshot.primary.max_stack = definition->get_max_stack();
        output_snapshot.primary.rarity = definition->get_rarity();
        output_snapshot.primary.width = definition->get_width();
        output_snapshot.primary.height = definition->get_height();
        output_snapshot.primary.modifier1_id = definition->get_modifier1_id();
        output_snapshot.primary.modifier1_value =
            definition->get_modifier1_value();
        output_snapshot.primary.modifier2_id = definition->get_modifier2_id();
        output_snapshot.primary.modifier2_value =
            definition->get_modifier2_value();
        output_snapshot.primary.modifier3_id = definition->get_modifier3_id();
        output_snapshot.primary.modifier3_value =
            definition->get_modifier3_value();
        output_snapshot.primary.modifier4_id = definition->get_modifier4_id();
        output_snapshot.primary.modifier4_value =
            definition->get_modifier4_value();
        output_snapshot.primary.instance_id = definition->get_instance_id();
        output_snapshot.byproduct_count = static_cast<uint32_t>(
            output_entry->value.byproduct_count);
        byproduct_index = 0U;
        while (byproduct_index < output_entry->value.byproduct_count)
        {
            const game_crafting_byproduct &source =
                output_entry->value.byproducts[byproduct_index];
            game_crafting_product_snapshot &destination =
                output_snapshot.byproducts[byproduct_index];

            destination.item_id = source.item_id;
            destination.quantity = source.quantity * batch_count;
            destination.max_stack = source.max_stack;
            destination.rarity = source.rarity;
            destination.width = source.width;
            destination.height = source.height;
            destination.modifier1_id = source.modifier1_id;
            destination.modifier1_value = source.modifier1_value;
            destination.modifier2_id = source.modifier2_id;
            destination.modifier2_value = source.modifier2_value;
            destination.modifier3_id = source.modifier3_id;
            destination.modifier3_value = source.modifier3_value;
            destination.modifier4_id = source.modifier4_id;
            destination.modifier4_value = source.modifier4_value;
            destination.instance_id = source.instance_id;
            ++byproduct_index;
        }
    }
    if (result == FT_ERR_SUCCESS && include_outputs != FT_FALSE)
    {
        ft_sharedptr<game_item> output_copy;
        result = game_crafting_clone_item(output_entry->value.item_definition,
            &output_copy);
        if (result == FT_ERR_SUCCESS)
        {
            output_copy->set_stack_size(output_entry->value.quantity * batch_count);
            candidate._primary_output_slot = staged._next_slot;
            candidate._primary_output_item_id =
                output_entry->value.item_definition->get_item_id();
            candidate._primary_output_quantity =
                output_entry->value.quantity * batch_count;
            result = staged.add_item(output_copy);
        }
    }
    byproduct_index = 0U;
    while (result == FT_ERR_SUCCESS && include_outputs != FT_FALSE
        && byproduct_index < output_entry->value.byproduct_count)
    {
        const game_crafting_byproduct &byproduct =
            output_entry->value.byproducts[byproduct_index];
        ft_sharedptr<game_item> byproduct_copy;

        result = byproduct_copy.initialize(new (std::nothrow) game_item());
        if (result == FT_ERR_SUCCESS && !byproduct_copy)
            result = FT_ERR_NO_MEMORY;
        if (result == FT_ERR_SUCCESS)
            result = byproduct_copy->initialize();
        if (result == FT_ERR_SUCCESS)
        {
            byproduct_copy->set_item_id(byproduct.item_id);
            byproduct_copy->set_max_stack(byproduct.max_stack);
            byproduct_copy->set_stack_size(byproduct.quantity * batch_count);
            byproduct_copy->set_rarity(byproduct.rarity);
            byproduct_copy->set_width(byproduct.width);
            byproduct_copy->set_height(byproduct.height);
            byproduct_copy->set_modifier1_id(byproduct.modifier1_id);
            byproduct_copy->set_modifier1_value(byproduct.modifier1_value);
            byproduct_copy->set_modifier2_id(byproduct.modifier2_id);
            byproduct_copy->set_modifier2_value(byproduct.modifier2_value);
            byproduct_copy->set_modifier3_id(byproduct.modifier3_id);
            byproduct_copy->set_modifier3_value(byproduct.modifier3_value);
            byproduct_copy->set_modifier4_id(byproduct.modifier4_id);
            byproduct_copy->set_modifier4_value(byproduct.modifier4_value);
            result = byproduct_copy->get_error();
        }
        if (result == FT_ERR_SUCCESS)
        {
            result = staged.add_item(byproduct_copy);
        }
        byproduct_index++;
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
    ft_size_t index;
    ft_size_t count;
    const Pair<int32_t, ft_sharedptr<game_item>> *expected;
    Pair<int32_t, ft_sharedptr<game_item>> *primary_output;
    Pair<int32_t, ft_sharedptr<game_item>> *existing;
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
        || inventory._revision != prepared->_expected_revision
        || inventory._items.size() != prepared->_expected_items.size())
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS
        && inventory._revision == std::numeric_limits<uint64_t>::max())
        result = FT_ERR_OUT_OF_RANGE;
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
        index = 0U;
        count = prepared->_expected_items.size();
        expected = prepared->_expected_items.end() - count;
        while (result == FT_ERR_SUCCESS && index < count)
        {
            if (expected->value
                && expected->value->get_instance_id() == output_instance_id)
                result = FT_ERR_ALREADY_EXISTS;
            expected++;
            index++;
        }
        index = 0U;
        count = prepared->_result_items.size();
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
            primary_output->value->set_instance_id(output_instance_id);
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
    if (this->_recipe_stations.find(recipe_id) != this->_recipe_stations.end())
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_PERMISSION_DENIED));
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
