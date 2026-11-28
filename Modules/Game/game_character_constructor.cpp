#include "game_character.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_quest.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"

thread_local int32_t game_character::_last_error = FT_ERR_SUCCESS;

ft_bool game_character::handle_component_error(int32_t error) noexcept
{
    if (error == FT_ERR_SUCCESS)
        return (FT_FALSE);
    this->set_error(error);
    return (FT_TRUE);
}

ft_bool game_character::check_internal_errors() noexcept
{
    if (this->handle_component_error(this->_equipment.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_fire_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_frost_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_lightning_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_air_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_earth_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_chaos_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    if (this->handle_component_error(this->_physical_res.get_error()) == FT_TRUE)
        return (FT_TRUE);
    return (FT_FALSE);
}

game_character::game_character() noexcept
    : _hit_points(0), _physical_armor(0), _magic_armor(0),
      _current_physical_armor(0), _current_magic_armor(0),
      _physical_damage_multiplier(1.0), _magic_damage_multiplier(1.0),
      _damage_rule(FT_DAMAGE_RULE_FLAT),
      _might(0), _agility(0), _endurance(0), _reason(0), _insigh(0), _presence(0),
      _coins(0), _valor(0), _experience(0), _x(0), _y(0), _z(0),
      _fire_res(), _frost_res(), _lightning_res(),
      _air_res(), _earth_res(), _chaos_res(),
      _physical_res(), _skills(), _buffs(), _debuffs(), _upgrades(), _quests(), _achievements(), _reputation(), _inventory(), _equipment(),
      _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED), _equipped_modifiers{}
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_character::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_character::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }

    int32_t slot = EQUIP_HEAD;
    int32_t initialize_error = this->_fire_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto initialize_failed;
    initialize_error = this->_frost_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_fire_res;
    initialize_error = this->_lightning_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_frost_res;
    initialize_error = this->_air_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_lightning_res;
    initialize_error = this->_earth_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_air_res;
    initialize_error = this->_chaos_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_earth_res;
    initialize_error = this->_physical_res.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_chaos_res;
    initialize_error = this->_reputation.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_physical_res;
    initialize_error = this->_inventory.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_reputation;
    initialize_error = this->_experience_table.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_inventory;
    initialize_error = this->_skills.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_experience_table;
    initialize_error = this->_buffs.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_skills;
    initialize_error = this->_debuffs.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_buffs;
    initialize_error = this->_upgrades.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_debuffs;
    initialize_error = this->_quests.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_upgrades;
    initialize_error = this->_achievements.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_quests;
    initialize_error = this->_equipment.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        goto destroy_achievements;

    while (slot <= EQUIP_OFF_HAND)
    {
        this->_equipped_modifiers[slot] = {};
        ++slot;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));

destroy_achievements:
    (void)this->_achievements.destroy();
destroy_quests:
    (void)this->_quests.destroy();
destroy_upgrades:
    (void)this->_upgrades.destroy();
destroy_debuffs:
    (void)this->_debuffs.destroy();
destroy_buffs:
    (void)this->_buffs.destroy();
destroy_skills:
    (void)this->_skills.destroy();
destroy_experience_table:
    (void)this->_experience_table.destroy();
destroy_inventory:
    (void)this->_inventory.destroy();
destroy_reputation:
    (void)this->_reputation.destroy();
destroy_physical_res:
    (void)this->_physical_res.destroy();
destroy_chaos_res:
    (void)this->_chaos_res.destroy();
destroy_earth_res:
    (void)this->_earth_res.destroy();
destroy_air_res:
    (void)this->_air_res.destroy();
destroy_lightning_res:
    (void)this->_lightning_res.destroy();
destroy_frost_res:
    (void)this->_frost_res.destroy();
destroy_fire_res:
    (void)this->_fire_res.destroy();
initialize_failed:
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(initialize_error));
}

game_character::~game_character() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_character::destroy() noexcept
{
    int32_t first_error;
    int32_t current_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    first_error = FT_ERR_SUCCESS;
    current_error = this->disable_thread_safety();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_achievements.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_quests.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_upgrades.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_debuffs.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_buffs.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_skills.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_experience_table.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_equipment.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_inventory.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_reputation.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_physical_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_chaos_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_earth_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_air_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_lightning_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_frost_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_fire_res.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(first_error);
    return (first_error);
}

template <typename Key, typename Value>
static int32_t check_character_move_map(const ft_map<Key, Value> &map) noexcept
{
    if (map.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (!map.is_thread_safe())
        return (FT_ERR_SUCCESS);
    if (map.has_active_lock())
        return (FT_ERR_THREAD_BUSY);
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = map.lock(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        (void)map.unlock(lock_acquired);
    return (result);
}

int32_t game_character::validate_move_source() const noexcept
{
    if (this->_inventory._initialised_state == FT_CLASS_STATE_INITIALISED
        && this->_inventory._revision == UINT64_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    const pt_recursive_mutex *const mutexes[] = {
        this->_mutex, this->_fire_res._mutex, this->_frost_res._mutex,
        this->_lightning_res._mutex, this->_air_res._mutex, this->_earth_res._mutex,
        this->_chaos_res._mutex, this->_physical_res._mutex, this->_reputation._mutex,
        this->_inventory._mutex, this->_equipment._mutex, this->_experience_table._mutex};
    ft_size_t mutex_index = 0;
    while (mutex_index < sizeof(mutexes) / sizeof(mutexes[0]))
    {
        const pt_recursive_mutex *mutex = mutexes[mutex_index];
        if (mutex != ft_nullptr)
        {
            if (mutex->lockState())
                return (FT_ERR_THREAD_BUSY);
            const int32_t result = pt_recursive_mutex_lock_if_not_null(mutex);
            if (result != FT_ERR_SUCCESS)
                return (result);
            (void)pt_recursive_mutex_unlock_if_not_null(mutex);
        }
        ++mutex_index;
    }

    int32_t result = check_character_move_map(this->_skills);
    if (result == FT_ERR_SUCCESS)
        result = check_character_move_map(this->_buffs);
    if (result == FT_ERR_SUCCESS)
        result = check_character_move_map(this->_debuffs);
    if (result == FT_ERR_SUCCESS)
        result = check_character_move_map(this->_upgrades);
    if (result == FT_ERR_SUCCESS)
        result = check_character_move_map(this->_quests);
    if (result == FT_ERR_SUCCESS)
        result = check_character_move_map(this->_achievements);
    if (result == FT_ERR_SUCCESS
        && this->_reputation._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        result = check_character_move_map(this->_reputation._milestones);
        if (result == FT_ERR_SUCCESS)
            result = check_character_move_map(this->_reputation._reps);
    }
    if (result == FT_ERR_SUCCESS
        && this->_inventory._initialised_state == FT_CLASS_STATE_INITIALISED)
        result = check_character_move_map(this->_inventory._items);
    return (result);
}

int32_t game_character::move(game_character &other) noexcept
{
    int32_t destroy_error;
    int32_t transfer_error;
    int32_t source_destroy_error;
    int32_t slot;

    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_character::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t source_error = other.get_error();
    if (this->_mutex != ft_nullptr && this->_mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    // Preserve the destination inventory revision sequence across the move.
    if (this->_inventory._revision > UINT64_MAX - 3U)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    transfer_error = other.validate_move_source();
    if (transfer_error != FT_ERR_SUCCESS)
        return (this->set_error(transfer_error));
    // Component moves initialize their destination directly without empty allocations.
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->_hit_points = other._hit_points;
    this->_physical_armor = other._physical_armor;
    this->_magic_armor = other._magic_armor;
    this->_current_physical_armor = other._current_physical_armor;
    this->_current_magic_armor = other._current_magic_armor;
    this->_damage_rule = other._damage_rule;
    this->_might = other._might;
    this->_agility = other._agility;
    this->_endurance = other._endurance;
    this->_reason = other._reason;
    this->_insigh = other._insigh;
    this->_presence = other._presence;
    this->_coins = other._coins;
    this->_valor = other._valor;
    this->_experience = other._experience;
    this->_x = other._x;
    this->_y = other._y;
    this->_z = other._z;
    this->_physical_damage_multiplier = other._physical_damage_multiplier;
    this->_magic_damage_multiplier = other._magic_damage_multiplier;
    transfer_error = this->_fire_res.move(other._fire_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_frost_res.move(other._frost_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_lightning_res.move(other._lightning_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_air_res.move(other._air_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_earth_res.move(other._earth_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_chaos_res.move(other._chaos_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_physical_res.move(other._physical_res);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_skills.move(other._skills);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_buffs.move(other._buffs);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_debuffs.move(other._debuffs);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_upgrades.move(other._upgrades);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_quests.move(other._quests);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_achievements.move(other._achievements);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_reputation.move(other._reputation);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_equipment.move(other._equipment);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_experience_table.move(other._experience_table);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    transfer_error = this->_inventory.move(other._inventory);
    if (transfer_error != FT_ERR_SUCCESS)
        goto transfer_failed;
    this->_inventory._revision += 2U;
    slot = EQUIP_HEAD;
    while (slot <= EQUIP_OFF_HAND)
    {
        this->_equipped_modifiers[slot] = other._equipped_modifiers[slot];
        ++slot;
    }
    source_destroy_error = other.destroy();
    if (source_destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(source_destroy_error));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);

transfer_failed:
    (void)this->destroy();
    return (this->set_error(transfer_error));
}

int32_t game_character::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t game_character::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    return (destroy_error);
}

ft_bool game_character::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}
