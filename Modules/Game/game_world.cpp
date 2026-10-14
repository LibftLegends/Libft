#include "game_world.hpp"
#include "game_character.hpp"
#include "game_inventory.hpp"
#include "game_world_registry.hpp"
#include "game_world_replay.hpp"
#include "game_economy_table.hpp"
#include "game_crafting.hpp"
#include "game_dialogue_table.hpp"
#include "game_world_region.hpp"
#include "game_quest.hpp"
#include "game_vendor_profile.hpp"
#include "game_upgrade.hpp"
#include "../JSon/json.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Template/function.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <utility>
#include <new>
#include "../Template/move.hpp"
#include "../Storage/kv_store.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_currency_rate.hpp"
#include "game_debuff.hpp"
#include "game_dialogue_line.hpp"
#include "game_dialogue_script.hpp"
#include "game_pathfinding.hpp"
#include "game_price_definition.hpp"
#include "game_rarity_band.hpp"
#include "game_region_definition.hpp"
#include "game_skill.hpp"

json_group *serialize_character(const game_character &character);
int32_t deserialize_character(game_character &character, json_group *group);
json_group *serialize_event_scheduler(const ft_sharedptr<game_event_scheduler> &scheduler);
int32_t deserialize_event_scheduler(ft_sharedptr<game_event_scheduler> &scheduler, json_group *group);
json_group *serialize_inventory(const game_inventory &inventory);
int32_t deserialize_inventory(game_inventory &inventory, json_group *group);
json_group *serialize_equipment(const game_character &character);
int32_t deserialize_equipment(game_character &character, json_group *group);

thread_local int32_t game_world::_last_error = FT_ERR_SUCCESS;

static void game_world_clear_event_handles(
    ft_vector<ft_sharedptr<game_event> > &events) noexcept
{
    events.clear();
    return ;
}

static void default_event_callback(game_world &world, game_event &event) noexcept;
static ft_function<void(game_world&, game_event&)> get_callback_by_id(int32_t type_id) noexcept;
static void destroy_world_lifecycle_components(
    ft_sharedptr<game_event_scheduler> &event_scheduler,
    ft_sharedptr<game_world_registry> &world_registry,
    ft_sharedptr<game_world_replay_session> &replay_session,
    ft_sharedptr<game_economy_table> &economy_table,
    ft_sharedptr<game_crafting> &crafting,
    ft_sharedptr<game_dialogue_table> &dialogue_table,
    ft_sharedptr<game_world_region> &world_region,
    ft_sharedptr<game_quest> &quest,
    ft_sharedptr<game_vendor_profile> &vendor_profile,
    ft_sharedptr<game_upgrade> &upgrade) noexcept;

static void default_event_callback(game_world &world, game_event &event) noexcept
{
    (void)world;
    (void)event;
    return ;
}

static ft_function<void(game_world&, game_event&)> get_callback_by_id(int32_t type_id) noexcept
{
    if (type_id == 1)
        return (ft_function<void(game_world&, game_event&)>(default_event_callback));
    return (ft_function<void(game_world&, game_event&)>());
}

static void destroy_world_lifecycle_components(
    ft_sharedptr<game_event_scheduler> &event_scheduler,
    ft_sharedptr<game_world_registry> &world_registry,
    ft_sharedptr<game_world_replay_session> &replay_session,
    ft_sharedptr<game_economy_table> &economy_table,
    ft_sharedptr<game_crafting> &crafting,
    ft_sharedptr<game_dialogue_table> &dialogue_table,
    ft_sharedptr<game_world_region> &world_region,
    ft_sharedptr<game_quest> &quest,
    ft_sharedptr<game_vendor_profile> &vendor_profile,
    ft_sharedptr<game_upgrade> &upgrade) noexcept
{
    if (replay_session.get() != ft_nullptr)
        (void)replay_session->destroy();
    if (upgrade.get() != ft_nullptr)
        (void)upgrade->destroy();
    if (vendor_profile.get() != ft_nullptr)
        (void)vendor_profile->destroy();
    if (quest.get() != ft_nullptr)
        (void)quest->destroy();
    if (world_region.get() != ft_nullptr)
        (void)world_region->destroy();
    if (dialogue_table.get() != ft_nullptr)
        (void)dialogue_table->destroy();
    if (crafting.get() != ft_nullptr)
        (void)crafting->destroy();
    if (economy_table.get() != ft_nullptr)
        (void)economy_table->destroy();
    if (world_registry.get() != ft_nullptr)
        (void)world_registry->destroy();
    if (event_scheduler.get() != ft_nullptr)
        (void)event_scheduler->destroy();
    event_scheduler.reset();
    world_registry.reset();
    replay_session.reset();
    economy_table.reset();
    crafting.reset();
    dialogue_table.reset();
    world_region.reset();
    quest.reset();
    vendor_profile.reset();
    upgrade.reset();
    return ;
}

ft_bool game_world::propagate_scheduler_state_error() const noexcept
{
    if (!this->_event_scheduler)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    int32_t scheduler_error = this->_event_scheduler->get_error();
    if (scheduler_error != FT_ERR_SUCCESS)
    {
        this->set_error(scheduler_error);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

game_world::game_world() noexcept
    : _event_scheduler(), _world_registry(), _replay_session(),
    _economy_table(), _crafting(), _dialogue_table(), _world_region(), _quest(),
    _vendor_profile(), _upgrade(),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_world::~game_world() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_world::initialize() noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_world::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_event_scheduler = ft_sharedptr<game_event_scheduler>(
        new (std::nothrow) game_event_scheduler());
    this->_world_registry = ft_sharedptr<game_world_registry>(
        new (std::nothrow) game_world_registry());
    this->_replay_session = ft_sharedptr<game_world_replay_session>(
        new (std::nothrow) game_world_replay_session());
    this->_economy_table = ft_sharedptr<game_economy_table>(
        new (std::nothrow) game_economy_table());
    this->_crafting = ft_sharedptr<game_crafting>(new (std::nothrow) game_crafting());
    this->_dialogue_table = ft_sharedptr<game_dialogue_table>(
        new (std::nothrow) game_dialogue_table());
    this->_world_region = ft_sharedptr<game_world_region>(
        new (std::nothrow) game_world_region());
    this->_quest = ft_sharedptr<game_quest>(new (std::nothrow) game_quest());
    this->_vendor_profile = ft_sharedptr<game_vendor_profile>(
        new (std::nothrow) game_vendor_profile());
    this->_upgrade = ft_sharedptr<game_upgrade>(new (std::nothrow) game_upgrade());
    if (!this->_event_scheduler || !this->_world_registry || !this->_replay_session
        || !this->_economy_table || !this->_crafting || !this->_dialogue_table
        || !this->_world_region || !this->_quest || !this->_vendor_profile
        || !this->_upgrade)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = this->_event_scheduler->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_replay_session->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_economy_table->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_crafting->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_dialogue_table->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_world_region->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_quest->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_vendor_profile->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_upgrade->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    initialize_error = this->_world_registry->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        destroy_world_lifecycle_components(this->_event_scheduler,
            this->_world_registry, this->_replay_session, this->_economy_table,
            this->_crafting, this->_dialogue_table, this->_world_region,
            this->_quest, this->_vendor_profile, this->_upgrade);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    destroy_world_lifecycle_components(this->_event_scheduler,
        this->_world_registry, this->_replay_session, this->_economy_table,
        this->_crafting, this->_dialogue_table, this->_world_region,
        this->_quest, this->_vendor_profile, this->_upgrade);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::move(game_world &other) noexcept
{
    int32_t destroy_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world::move", "source object is uninitialised");
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
    this->_event_scheduler = other._event_scheduler;
    this->_world_registry = other._world_registry;
    this->_replay_session = other._replay_session;
    this->_economy_table = other._economy_table;
    this->_crafting = other._crafting;
    this->_dialogue_table = other._dialogue_table;
    this->_world_region = other._world_region;
    this->_quest = other._quest;
    this->_vendor_profile = other._vendor_profile;
    this->_upgrade = other._upgrade;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._event_scheduler.reset();
    other._world_registry.reset();
    other._replay_session.reset();
    other._economy_table.reset();
    other._crafting.reset();
    other._dialogue_table.reset();
    other._world_region.reset();
    other._quest.reset();
    other._vendor_profile.reset();
    other._upgrade.reset();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

void game_world::schedule_event(const ft_sharedptr<game_event> &event) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::schedule_event");
    if (!event)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return ;
    this->_event_scheduler->schedule_event(event);
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return ;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_world::update_events(ft_sharedptr<game_world> &self, int32_t ticks, const char *log_file_path, ft_string *log_buffer) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::update_events");
    if (!self)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return ;
    this->_event_scheduler->update_events(self, ticks, log_file_path, log_buffer);
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return ;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_sharedptr<game_event_scheduler> &game_world::get_event_scheduler() noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_event_scheduler");
    previous_error = this->get_error();
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_event_scheduler);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_event_scheduler);
}

const ft_sharedptr<game_event_scheduler> &game_world::get_event_scheduler() const noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_event_scheduler const");
    previous_error = this->get_error();
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_event_scheduler);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_event_scheduler);
}

ft_sharedptr<game_world_registry> &game_world::get_world_registry() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_world_registry");
    if (this->propagate_registry_state_error() == FT_TRUE)
        return (this->_world_registry);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_registry);
}

const ft_sharedptr<game_world_registry> &game_world::get_world_registry() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_world_registry const");
    if (this->propagate_registry_state_error() == FT_TRUE)
        return (this->_world_registry);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_registry);
}

ft_sharedptr<game_world_replay_session> &game_world::get_replay_session() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_replay_session");
    if (this->propagate_replay_state_error() == FT_TRUE)
        return (this->_replay_session);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_replay_session);
}

const ft_sharedptr<game_world_replay_session> &game_world::get_replay_session() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_replay_session const");
    if (this->propagate_replay_state_error() == FT_TRUE)
        return (this->_replay_session);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_replay_session);
}

ft_sharedptr<game_economy_table> &game_world::get_economy_table() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_economy_table");
    if (this->propagate_economy_state_error() == FT_TRUE)
        return (this->_economy_table);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_economy_table);
}

const ft_sharedptr<game_economy_table> &game_world::get_economy_table() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_economy_table const");
    if (this->propagate_economy_state_error() == FT_TRUE)
        return (this->_economy_table);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_economy_table);
}

ft_sharedptr<game_crafting> &game_world::get_crafting() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_crafting");
    if (this->propagate_crafting_state_error() == FT_TRUE)
        return (this->_crafting);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_crafting);
}

const ft_sharedptr<game_crafting> &game_world::get_crafting() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_crafting const");
    if (this->propagate_crafting_state_error() == FT_TRUE)
        return (this->_crafting);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_crafting);
}

ft_sharedptr<game_dialogue_table> &game_world::get_dialogue_table() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_dialogue_table");
    if (this->propagate_dialogue_state_error() == FT_TRUE)
        return (this->_dialogue_table);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_dialogue_table);
}

const ft_sharedptr<game_dialogue_table> &game_world::get_dialogue_table() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_dialogue_table const");
    if (this->propagate_dialogue_state_error() == FT_TRUE)
        return (this->_dialogue_table);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_dialogue_table);
}

ft_sharedptr<game_world_region> &game_world::get_world_region() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_world_region");
    if (this->propagate_region_state_error() == FT_TRUE)
        return (this->_world_region);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_region);
}

const ft_sharedptr<game_world_region> &game_world::get_world_region() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_world_region const");
    if (this->propagate_region_state_error() == FT_TRUE)
        return (this->_world_region);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_region);
}

ft_sharedptr<game_quest> &game_world::get_quest() noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_quest");
    previous_error = this->get_error();
    if (this->propagate_quest_state_error() == FT_TRUE)
        return (this->_quest);
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_quest);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_quest);
}

const ft_sharedptr<game_quest> &game_world::get_quest() const noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_quest const");
    previous_error = this->get_error();
    if (this->propagate_quest_state_error() == FT_TRUE)
        return (this->_quest);
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_quest);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_quest);
}

ft_sharedptr<game_vendor_profile> &game_world::get_vendor_profile() noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_vendor_profile");
    previous_error = this->get_error();
    if (this->propagate_vendor_profile_state_error() == FT_TRUE)
        return (this->_vendor_profile);
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_vendor_profile);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_vendor_profile);
}

const ft_sharedptr<game_vendor_profile> &game_world::get_vendor_profile() const noexcept
{
    int32_t previous_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_vendor_profile const");
    previous_error = this->get_error();
    if (this->propagate_vendor_profile_state_error() == FT_TRUE)
        return (this->_vendor_profile);
    if (previous_error != FT_ERR_SUCCESS)
    {
        this->set_error(previous_error);
        return (this->_vendor_profile);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (this->_vendor_profile);
}

ft_sharedptr<game_upgrade> &game_world::get_upgrade() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_upgrade");
    if (this->propagate_upgrade_state_error() == FT_TRUE)
        return (this->_upgrade);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_upgrade);
}

const ft_sharedptr<game_upgrade> &game_world::get_upgrade() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::get_upgrade const");
    if (this->propagate_upgrade_state_error() == FT_TRUE)
        return (this->_upgrade);
    this->set_error(FT_ERR_SUCCESS);
    return (this->_upgrade);
}

int32_t game_world::save_to_file(const char *file_path, const game_character &character, const game_inventory &inventory) const noexcept
{
    json_group *groups;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::save_to_file");
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    error_code = FT_ERR_SUCCESS;
    groups = this->build_snapshot_groups(character, inventory, error_code);
    if (!groups)
        return (this->get_error());
    if (json_write_to_file(file_path, groups) != 0)
    {
        json_free_groups(groups);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    json_free_groups(groups);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::load_from_file(const char *file_path, game_character &character, game_inventory &inventory) noexcept
{
    json_group *groups;
    int32_t restore_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::load_from_file");
    groups = json_read_from_file(file_path);
    if (!groups)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    restore_result = this->restore_from_groups(groups, character, inventory);
    json_free_groups(groups);
    return (restore_result);
}

int32_t game_world::save_to_store(kv_store &store, const char *slot_key, const game_character &character, const game_inventory &inventory) const noexcept
{
    json_group *groups;
    char *serialized_state;
    int32_t error_code;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::save_to_store");
    if (slot_key == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (this->get_error());
    }
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    error_code = FT_ERR_SUCCESS;
    groups = this->build_snapshot_groups(character, inventory, error_code);
    if (!groups)
        return (this->get_error());
    serialized_state = json_write_to_string(groups);
    json_free_groups(groups);
    if (!serialized_state)
    {
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_NO_MEMORY;
        this->set_error(error_code);
        return (this->get_error());
    }
    if (store.kv_set(slot_key, serialized_state) != 0)
    {
        cma_free(serialized_state);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    cma_free(serialized_state);
    if (store.kv_flush() != 0)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::load_from_store(kv_store &store, const char *slot_key, game_character &character, game_inventory &inventory) noexcept
{
    const char *serialized_state;
    json_group *groups;
    int32_t restore_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::load_from_store");
    if (slot_key == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (this->get_error());
    }
    serialized_state = store.kv_get(slot_key);
    if (serialized_state == ft_nullptr)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    groups = json_read_from_string(serialized_state);
    if (!groups)
    {
        int32_t parse_error;

        parse_error = FT_ERR_GAME_GENERAL_ERROR;
        if (parse_error == FT_ERR_SUCCESS)
            parse_error = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(parse_error);
        return (this->get_error());
    }
    restore_result = this->restore_from_groups(groups, character, inventory);
    json_free_groups(groups);
    return (restore_result);
}

int32_t game_world::save_to_buffer(ft_string &out_buffer, const game_character &character, const game_inventory &inventory) const noexcept
{
    json_group *groups;
    char *serialized_state;
    int32_t error_code;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::save_to_buffer");
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    error_code = FT_ERR_SUCCESS;
    groups = this->build_snapshot_groups(character, inventory, error_code);
    if (!groups)
        return (this->get_error());
    serialized_state = json_write_to_string(groups);
    json_free_groups(groups);
    if (!serialized_state)
    {
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(error_code);
        return (this->get_error());
    }
    out_buffer = serialized_state;
    if (out_buffer.get_error() != FT_ERR_SUCCESS)
    {
        int32_t assign_error;

        assign_error = out_buffer.get_error();
        cma_free(serialized_state);
        this->set_error(assign_error);
        return (this->get_error());
    }
    cma_free(serialized_state);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::load_from_buffer(const char *buffer, game_character &character, game_inventory &inventory) noexcept
{
    json_group *groups;
    int32_t restore_result;
    int32_t parse_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::load_from_buffer");
    if (buffer == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (this->get_error());
    }
    groups = json_read_from_string(buffer);
    if (!groups)
    {
        parse_error = FT_ERR_GAME_GENERAL_ERROR;
        if (parse_error == FT_ERR_SUCCESS)
            parse_error = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(parse_error);
        return (this->get_error());
    }
    restore_result = this->restore_from_groups(groups, character, inventory);
    json_free_groups(groups);
    return (restore_result);
}

int32_t game_world::plan_route(const game_map3d &grid,
    ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
    ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
    ft_vector<game_path_step> &path) const noexcept
{
    game_pathfinding finder;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world::plan_route");
    if (start_x >= grid.get_width() || start_y >= grid.get_height()
        || start_z >= grid.get_depth() || goal_x >= grid.get_width()
        || goal_y >= grid.get_height() || goal_z >= grid.get_depth())
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (finder.astar_grid(grid, start_x, start_y, start_z,
            goal_x, goal_y, goal_z, path) != FT_ERR_SUCCESS)
    {
        this->set_error(finder.get_error());
        return (this->get_error());
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world::get_error");
    return (game_world::_last_error);
}

const char *game_world::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world::get_error_str");
    return (ft_strerror(game_world::_last_error));
}

int32_t game_world::set_error(int32_t error_code) noexcept
{
    game_world::_last_error = error_code;
    return (error_code);
}

ft_bool game_world::propagate_registry_state_error() const noexcept
{
    if (!this->_world_registry)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_replay_state_error() const noexcept
{
    if (!this->_replay_session)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_economy_state_error() const noexcept
{
    if (!this->_economy_table)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_crafting_state_error() const noexcept
{
    if (!this->_crafting)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    int32_t crafting_error;

    crafting_error = this->_crafting->get_error();
    if (crafting_error != FT_ERR_SUCCESS)
    {
        this->set_error(crafting_error);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_dialogue_state_error() const noexcept
{
    if (!this->_dialogue_table)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_region_state_error() const noexcept
{
    if (!this->_world_region)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_quest_state_error() const noexcept
{
    if (!this->_quest)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_vendor_profile_state_error() const noexcept
{
    if (!this->_vendor_profile)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

ft_bool game_world::propagate_upgrade_state_error() const noexcept
{
    if (!this->_upgrade)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}
json_group *game_world::build_snapshot_groups(const game_character &character,
    const game_inventory &inventory, int32_t &error_code) const noexcept
{
    json_group *groups;
    json_group *event_group;
    json_group *character_group;
    json_group *inventory_group;
    json_group *equipment_group;

    groups = ft_nullptr;
    error_code = FT_ERR_SUCCESS;
    event_group = serialize_event_scheduler(this->_event_scheduler);
    if (!event_group)
    {
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(error_code);
        return (ft_nullptr);
    }
    json_append_group(&groups, event_group);
    character_group = serialize_character(character);
    if (!character_group)
    {
        json_free_groups(groups);
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(error_code);
        return (ft_nullptr);
    }
    json_append_group(&groups, character_group);
    inventory_group = serialize_inventory(inventory);
    if (!inventory_group)
    {
        json_free_groups(groups);
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(error_code);
        return (ft_nullptr);
    }
    json_append_group(&groups, inventory_group);
    equipment_group = serialize_equipment(character);
    if (!equipment_group)
    {
        json_free_groups(groups);
        error_code = FT_ERR_GAME_GENERAL_ERROR;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_GAME_GENERAL_ERROR;
        this->set_error(error_code);
        return (ft_nullptr);
    }
    json_append_group(&groups, equipment_group);
    this->set_error(FT_ERR_SUCCESS);
    error_code = FT_ERR_SUCCESS;
    return (groups);
}

int32_t game_world::restore_from_groups(json_group *groups, game_character &character,
    game_inventory &inventory) noexcept
{
    json_group *event_group;
    json_group *character_group;
    json_group *inventory_group;
    json_group *equipment_group;

    if (!groups)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    event_group = json_find_group(groups, "world");
    character_group = json_find_group(groups, "character");
    inventory_group = json_find_group(groups, "inventory");
    equipment_group = json_find_group(groups, "equipment");
    if (!event_group || !character_group || !inventory_group || !equipment_group)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    this->_event_scheduler->clear();
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    if (inventory.clear_items() != FT_ERR_SUCCESS)
    {
        this->set_error(inventory.get_error());
        return (this->get_error());
    }
    if (deserialize_event_scheduler(this->_event_scheduler, event_group) != FT_ERR_SUCCESS ||
        deserialize_character(character, character_group) != FT_ERR_SUCCESS ||
        deserialize_inventory(inventory, inventory_group) != FT_ERR_SUCCESS ||
        deserialize_equipment(character, equipment_group) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (this->get_error());
    }
    ft_vector<ft_sharedptr<game_event> > scheduled_events;
    if (scheduled_events.initialize() != FT_ERR_SUCCESS)
    {
        this->set_error(scheduled_events.get_error());
        return (this->get_error());
    }
    this->_event_scheduler->dump_events(scheduled_events);
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    this->_event_scheduler->clear();
    if (this->propagate_scheduler_state_error() == FT_TRUE)
        return (this->get_error());
    ft_size_t event_index;
    ft_size_t event_count;

    event_index = 0;
    event_count = scheduled_events.size();
    while (event_index < event_count)
    {
        ft_sharedptr<game_event> &scheduled_event = scheduled_events[event_index];

        if (!scheduled_event)
        {
            event_index++;
            continue ;
        }
        scheduled_event->set_callback(get_callback_by_id(scheduled_event->get_id()));
        this->_event_scheduler->schedule_event(scheduled_event);
        if (this->propagate_scheduler_state_error() == FT_TRUE)
        {
            game_world_clear_event_handles(scheduled_events);
            return (this->get_error());
        }
        event_index++;
    }
    game_world_clear_event_handles(scheduled_events);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}
