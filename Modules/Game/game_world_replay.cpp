#include "game_world_replay.hpp"

#include "../Errno/errno.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/move.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
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

thread_local int32_t game_world_replay_session::_last_error = FT_ERR_SUCCESS;

static void world_replay_clear_event_handles(
    ft_vector<ft_sharedptr<game_event> > &events) noexcept
{
    events.clear();
    return ;
}

static int32_t world_replay_collect_callbacks(game_world &world,
    ft_vector<ft_function<void(game_world&, game_event&)> > &callbacks) noexcept
{
    ft_sharedptr<game_event_scheduler> &scheduler = world.get_event_scheduler();

    if (!scheduler)
        return (FT_ERR_GAME_GENERAL_ERROR);
    ft_vector<ft_sharedptr<game_event> > scheduled_events;
    if (scheduled_events.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);

    scheduler->dump_events(scheduled_events);
    if (scheduler->get_error() != FT_ERR_SUCCESS)
    {
        world_replay_clear_event_handles(scheduled_events);
        return (scheduler->get_error());
    }
    ft_size_t event_index;
    ft_size_t event_count;

    event_index = 0;
    event_count = scheduled_events.size();
    while (event_index < event_count)
    {
        int32_t event_error;

        if (!scheduled_events[event_index])
        {
            world_replay_clear_event_handles(scheduled_events);
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        const ft_function<void(game_world&, game_event&)> &event_callback =
            scheduled_events[event_index]->get_callback();

        event_error = scheduled_events[event_index]->get_error();
        if (event_error != FT_ERR_SUCCESS)
        {
            world_replay_clear_event_handles(scheduled_events);
            return (event_error);
        }
        callbacks.push_back(event_callback);
        if (callbacks.get_error() != FT_ERR_SUCCESS)
        {
            world_replay_clear_event_handles(scheduled_events);
            return (callbacks.get_error());
        }
        event_index++;
    }
    world_replay_clear_event_handles(scheduled_events);
    return (FT_ERR_SUCCESS);
}

static int32_t world_replay_restore_callbacks(game_world &world,
    const ft_vector<ft_function<void(game_world&, game_event&)> > &callbacks) noexcept
{
    ft_sharedptr<game_event_scheduler> &scheduler = world.get_event_scheduler();

    if (!scheduler)
        return (FT_ERR_GAME_GENERAL_ERROR);
    if (callbacks.size() == 0)
        return (FT_ERR_SUCCESS);
    ft_vector<ft_sharedptr<game_event> > scheduled_events;
    if (scheduled_events.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);

    scheduler->dump_events(scheduled_events);
    if (scheduler->get_error() != FT_ERR_SUCCESS)
    {
        world_replay_clear_event_handles(scheduled_events);
        return (scheduler->get_error());
    }
    ft_size_t event_index;
    ft_size_t event_count;
    ft_size_t callback_count;

    callback_count = callbacks.size();
    event_index = 0;
    event_count = scheduled_events.size();
    if (callback_count != event_count)
    {
        world_replay_clear_event_handles(scheduled_events);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    while (event_index < event_count)
    {
        ft_function<void(game_world&, game_event&)> callback_copy(callbacks[event_index]);
        int32_t event_error;

        if (!scheduled_events[event_index])
        {
            world_replay_clear_event_handles(scheduled_events);
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        scheduled_events[event_index]->set_callback(ft_move(callback_copy));
        event_error = scheduled_events[event_index]->get_error();
        if (event_error != FT_ERR_SUCCESS)
        {
            world_replay_clear_event_handles(scheduled_events);
            return (event_error);
        }
        event_index++;
    }
    world_replay_clear_event_handles(scheduled_events);
    return (FT_ERR_SUCCESS);
}

game_world_replay_session::game_world_replay_session() noexcept
    : _snapshot_payload(), _event_callbacks(),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_world_replay_session::~game_world_replay_session() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_world_replay_session::initialize() noexcept
{
    int32_t snapshot_error;
    int32_t callbacks_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_world_replay_session::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    snapshot_error = this->_snapshot_payload.initialize();
    if (snapshot_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(snapshot_error));
    }
    callbacks_error = this->_event_callbacks.initialize();
    if (callbacks_error != FT_ERR_SUCCESS)
    {
        (void)this->_snapshot_payload.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(callbacks_error));
    }
    this->_event_callbacks.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_world_replay_session::set_error(int32_t error_code) noexcept
{
    game_world_replay_session::_last_error = error_code;
    return (error_code);
}

int32_t game_world_replay_session::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_replay_session::get_error");
    return (game_world_replay_session::_last_error);
}

const char *game_world_replay_session::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_replay_session::get_error_str");
    return (ft_strerror(game_world_replay_session::_last_error));
}

int32_t game_world_replay_session::move(game_world_replay_session &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world_replay_session::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(other.get_error()));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (this->set_error(initialize_error));
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (this->set_error(initialize_error));
    this->_snapshot_payload = other._snapshot_payload;
    if (this->_snapshot_payload.get_error() != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(this->_snapshot_payload.get_error()));
    }
    this->_event_callbacks.clear();
    ft_size_t callback_index;
    ft_size_t callback_count;

    callback_index = 0;
    callback_count = other._event_callbacks.size();
    while (callback_index < callback_count)
    {
        this->_event_callbacks.push_back(other._event_callbacks[callback_index]);
        if (this->_event_callbacks.get_error() != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(this->_event_callbacks.get_error()));
        }
        callback_index++;
    }
    other._snapshot_payload.clear();
    other._event_callbacks.clear();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_world_replay_session::destroy() noexcept
{
    int32_t callbacks_error;
    int32_t snapshot_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    this->_event_callbacks.clear();
    callbacks_error = this->_event_callbacks.destroy();
    snapshot_error = this->_snapshot_payload.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (callbacks_error != FT_ERR_SUCCESS)
        return (this->set_error(callbacks_error));
    return (this->set_error(snapshot_error));
}

int32_t game_world_replay_session::capture_snapshot(game_world &world,
    const game_character &character, const game_inventory &inventory) noexcept
{
    ft_string snapshot_buffer;
    int32_t result;
    ft_vector<ft_function<void(game_world&, game_event&)> > callback_snapshot;
    int32_t callback_result;
    ft_size_t callback_index;
    ft_size_t callback_count;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::capture_snapshot");
    result = snapshot_buffer.initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    callback_result = callback_snapshot.initialize();
    if (callback_result != FT_ERR_SUCCESS)
        return (this->set_error(callback_result));
    result = world.save_to_buffer(snapshot_buffer, character, inventory);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    callback_result = world_replay_collect_callbacks(world, callback_snapshot);
    if (callback_result != FT_ERR_SUCCESS)
        return (this->set_error(callback_result));
    this->_snapshot_payload = snapshot_buffer;
    if (this->_snapshot_payload.get_error() != FT_ERR_SUCCESS)
        return (this->set_error(this->_snapshot_payload.get_error()));
    this->_event_callbacks.clear();
    callback_index = 0;
    callback_count = callback_snapshot.size();
    while (callback_index < callback_count)
    {
        this->_event_callbacks.push_back(callback_snapshot[callback_index]);
        if (this->_event_callbacks.get_error() != FT_ERR_SUCCESS)
            return (this->set_error(this->_event_callbacks.get_error()));
        callback_index++;
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_world_replay_session::restore_snapshot(ft_sharedptr<game_world> &world_ptr,
    game_character &character, game_inventory &inventory) noexcept
{
    int32_t load_result;
    int32_t callback_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::restore_snapshot");
    if (!world_ptr)
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    if (this->_snapshot_payload.empty())
        return (this->set_error(FT_ERR_INVALID_STATE));
    load_result = world_ptr->load_from_buffer(this->_snapshot_payload.c_str(),
            character, inventory);
    if (load_result != FT_ERR_SUCCESS)
        return (this->set_error(load_result));
    if (this->_event_callbacks.size() > 0)
    {
        callback_result = world_replay_restore_callbacks(*world_ptr,
                this->_event_callbacks);
        if (callback_result != FT_ERR_SUCCESS)
            return (this->set_error(callback_result));
    }
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_world_replay_session::replay_ticks(ft_sharedptr<game_world> &world_ptr,
    game_character &character, game_inventory &inventory, int32_t ticks,
    const char *log_file_path, ft_string *log_buffer) noexcept
{
    int32_t restore_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::replay_ticks");
    restore_result = this->restore_snapshot(world_ptr, character, inventory);
    if (restore_result != FT_ERR_SUCCESS)
        return (this->set_error(restore_result));
    world_ptr->update_events(world_ptr, ticks, log_file_path, log_buffer);
    return (this->set_error(world_ptr->get_error()));
}

int32_t game_world_replay_session::plan_route(game_world &world, const game_map3d &grid,
    ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
    ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
    ft_vector<game_path_step> &path) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::plan_route");
    return (this->set_error(world.plan_route(grid, start_x, start_y, start_z,
            goal_x, goal_y, goal_z, path)));
}

int32_t game_world_replay_session::import_snapshot(const ft_string &snapshot_payload)
    noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::import_snapshot");
    this->_snapshot_payload = snapshot_payload;
    if (this->_snapshot_payload.get_error() != FT_ERR_SUCCESS)
        return (this->set_error(this->_snapshot_payload.get_error()));
    this->_event_callbacks.clear();
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_world_replay_session::export_snapshot(ft_string &out_snapshot) const
    noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::export_snapshot");
    out_snapshot = this->_snapshot_payload;
    if (out_snapshot.get_error() != FT_ERR_SUCCESS)
        return (this->set_error(out_snapshot.get_error()));
    return (this->set_error(FT_ERR_SUCCESS));
}

void game_world_replay_session::clear_snapshot() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_replay_session::clear_snapshot");
    this->_snapshot_payload.clear();
    this->_event_callbacks.clear();
    this->set_error(FT_ERR_SUCCESS);
    return ;
}
