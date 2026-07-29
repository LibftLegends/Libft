#include "../PThread/pthread_internal.hpp"
#include "game_server.hpp"
#include "../JSon/document.hpp"
#include "../Networking/socket_class.hpp"
#include "../Template/pair.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
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

thread_local int32_t game_server::_last_error = FT_ERR_SUCCESS;

int32_t game_server::set_error(int32_t error_code) noexcept
{
    game_server::_last_error = error_code;
    return (error_code);
}

int32_t game_server::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_server::get_error");
    return (game_server::_last_error);
}

const char *game_server::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_server::get_error_str");
    return (ft_strerror(game_server::_last_error));
}

game_server::game_server() noexcept
    : _server(ft_nullptr), _world(), _clients(), _auth_token(),
      _on_join(ft_nullptr), _on_leave(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_server::initialize() noexcept
{
    ft_sharedptr<game_world> world;
    int32_t world_error;

    world_error = world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    return (this->initialize(world, ft_nullptr));
}

int32_t game_server::initialize(const ft_sharedptr<game_world> &world,
    const char *auth_token) noexcept
{
    ft_websocket_server *server_instance;
    int32_t server_initialize_error;
    int32_t world_error;
    int32_t clients_error;
    int32_t auth_token_destroy_error;
    int32_t clients_destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_server::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    world_error = this->_world.initialize(world);
    if (world_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    auth_token_destroy_error = this->_auth_token.destroy();
    if (auth_token_destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(auth_token_destroy_error);
        return (auth_token_destroy_error);
    }
    clients_destroy_error = this->_clients.destroy();
    if (clients_destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(clients_destroy_error);
        return (clients_destroy_error);
    }
    if (this->_auth_token.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(this->_auth_token.get_error());
        return (this->get_error());
    }
    clients_error = this->_clients.initialize();
    if (clients_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        (void)this->_auth_token.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(clients_error);
        return (this->get_error());
    }
    this->_auth_token.clear();
    this->_clients.clear();
    this->_on_join = ft_nullptr;
    this->_on_leave = ft_nullptr;
    if (this->_server != ft_nullptr)
    {
        delete this->_server;
        this->_server = ft_nullptr;
    }
    server_instance = new (std::nothrow) ft_websocket_server();
    if (server_instance == ft_nullptr)
    {
        (void)this->_clients.destroy();
        (void)this->_auth_token.destroy();
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    server_initialize_error = server_instance->initialize();
    if (server_initialize_error != FT_ERR_SUCCESS)
    {
        delete server_instance;
        (void)this->_clients.destroy();
        (void)this->_auth_token.destroy();
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(server_initialize_error);
        return (server_initialize_error);
    }
    this->_server = server_instance;
    if (auth_token != ft_nullptr)
        this->_auth_token = auth_token;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_server::destroy() noexcept
{
    int32_t disable_error;
    int32_t world_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    if (this->_server != ft_nullptr)
    {
        delete this->_server;
        this->_server = ft_nullptr;
    }
    world_destroy_error = this->_world.destroy();
    this->_clients.clear();
    this->_auth_token.clear();
    this->_on_join = ft_nullptr;
    this->_on_leave = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error == FT_ERR_SUCCESS && world_destroy_error != FT_ERR_SUCCESS)
        disable_error = world_destroy_error;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_server::move(game_server &other) noexcept
{
    int32_t destroy_error;
    int32_t source_error;
    int32_t source_destroy_error;
    int32_t move_error;
    ft_websocket_server *server_pointer;
    pt_recursive_mutex *mutex_pointer;
    ft_sharedptr<game_world> world_copy;
    ft_map<int32_t, int32_t> clients_copy;
    ft_string auth_token_copy;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_server::move", "source object is uninitialised");
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
    move_error = world_copy.initialize(other._world);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = clients_copy.copy_from(other._clients);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = auth_token_copy.initialize(other._auth_token);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = this->_world.move(world_copy);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = this->_clients.move(clients_copy);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = this->_auth_token.move(auth_token_copy);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        (void)this->_clients.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    if (other._mutex != ft_nullptr && other._mutex->lockState())
    {
        (void)this->_world.destroy();
        (void)this->_clients.destroy();
        (void)this->_auth_token.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_THREAD_BUSY);
        return (FT_ERR_THREAD_BUSY);
    }
    server_pointer = other._server;
    mutex_pointer = other._mutex;
    source_error = other.get_error();
    other._server = ft_nullptr;
    other._mutex = ft_nullptr;
    this->_server = server_pointer;
    this->_on_join = other._on_join;
    this->_on_leave = other._on_leave;
    this->_mutex = mutex_pointer;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    source_destroy_error = other.destroy();
    if (source_destroy_error != FT_ERR_SUCCESS)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
        this->set_error(source_destroy_error);
        return (source_destroy_error);
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

game_server::~game_server() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_server::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::lock_internal");
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

int32_t game_server::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);

    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_server::start(const char *ip, uint16_t port) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t start_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::start");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (this->_server == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    start_result = this->_server->start(ip, port, AF_INET, FT_FALSE);
    (void)this->unlock_internal(lock_acquired);
    if (start_result != 0)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void game_server::set_join_callback(void (*callback)(int32_t)) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::set_join_callback");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_on_join = callback;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_server::set_leave_callback(void (*callback)(int32_t)) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::set_leave_callback");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_on_leave = callback;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_server::handle_message_locked(int32_t client_handle,
    const ft_string &message) noexcept
{
    json_group *groups;
    json_group *join_group;
    json_group *leave_group;
    json_group *event_group;
    json_item *id_item;
    json_item *token_item;
    json_item *duration_item;
    int32_t client_id;
    ft_sharedptr<game_event> event;
    ft_sharedptr<game_event_scheduler> scheduler;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::handle_message_locked");
    groups = json_read_from_string(message.c_str());
    if (groups == ft_nullptr)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    join_group = json_find_group(groups, "join");
    if (join_group != ft_nullptr)
    {
        id_item = json_find_item(join_group, "id");
        token_item = json_find_item(join_group, "token");
        if (id_item == ft_nullptr)
        {
            json_free_groups(groups);
            this->set_error(FT_ERR_GAME_GENERAL_ERROR);
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        if (this->_auth_token.size() > 0)
        {
            if (token_item == ft_nullptr || this->_auth_token != token_item->value)
            {
                json_free_groups(groups);
                nw_close(client_handle);
                this->set_error(FT_ERR_GAME_GENERAL_ERROR);
                return (FT_ERR_GAME_GENERAL_ERROR);
            }
        }
        client_id = ft_atoi(id_item->value);
        this->join_client_locked(client_id, client_handle);
        json_free_groups(groups);
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    leave_group = json_find_group(groups, "leave");
    if (leave_group != ft_nullptr)
    {
        id_item = json_find_item(leave_group, "id");
        if (id_item == ft_nullptr)
        {
            json_free_groups(groups);
            this->set_error(FT_ERR_GAME_GENERAL_ERROR);
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        client_id = ft_atoi(id_item->value);
        this->leave_client_locked(client_id);
        nw_close(client_handle);
        json_free_groups(groups);
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    event_group = json_find_group(groups, "event");
    if (event_group == ft_nullptr)
    {
        json_free_groups(groups);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    id_item = json_find_item(event_group, "id");
    duration_item = json_find_item(event_group, "duration");
    if (id_item == ft_nullptr || duration_item == ft_nullptr)
    {
        json_free_groups(groups);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    event = ft_sharedptr<game_event>(new (std::nothrow) game_event());
    if (!event)
    {
        json_free_groups(groups);
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    if (event->initialize() != FT_ERR_SUCCESS)
    {
        json_free_groups(groups);
        this->set_error(event->get_error());
        return (this->get_error());
    }
    event->set_id(ft_atoi(id_item->value));
    if (event->get_error() != FT_ERR_SUCCESS)
    {
        json_free_groups(groups);
        this->set_error(event->get_error());
        return (this->get_error());
    }
    event->set_duration(ft_atoi(duration_item->value));
    if (event->get_error() != FT_ERR_SUCCESS)
    {
        json_free_groups(groups);
        this->set_error(event->get_error());
        return (this->get_error());
    }
    if (!this->_world)
    {
        json_free_groups(groups);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    this->_world->schedule_event(event);
    if (this->_world->get_error() != FT_ERR_SUCCESS)
    {
        json_free_groups(groups);
        this->set_error(this->_world->get_error());
        return (this->get_error());
    }
    json_free_groups(groups);
    scheduler = this->_world->get_event_scheduler();
    if (!scheduler)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_server::serialize_world_locked(ft_string &out) const noexcept
{
    ft_sharedptr<game_event_scheduler> scheduler;
    json_document document;
    json_group *group;
    char *content;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::serialize_world_locked");
    if (!this->_world)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    scheduler = this->_world->get_event_scheduler();
    if (!scheduler)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    group = serialize_event_scheduler(scheduler);
    if (group == ft_nullptr)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    document.append_group(group);
    content = document.write_to_string();
    if (content == ft_nullptr)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    out = content;
    cma_free(content);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void game_server::run_once() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t client_handle;
    ft_string message;
    ft_string update;
    Pair<int32_t, int32_t> *client_ptr;
    Pair<int32_t, int32_t> *client_end;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::run_once");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (this->_server == ft_nullptr)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    if (this->_server->run_once(client_handle, message) != 0)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    if (this->handle_message_locked(client_handle, message) != FT_ERR_SUCCESS)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    if (this->serialize_world_locked(update) != FT_ERR_SUCCESS)
    {
        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    client_ptr = this->_clients.end() - this->_clients.size();
    client_end = this->_clients.end();
    while (client_ptr != client_end)
    {
        int32_t send_error;

        send_error = this->_server->send_text(client_ptr->value, update);
        if (send_error != FT_ERR_SUCCESS)
        {
            (void)this->unlock_internal(lock_acquired);
            this->set_error(send_error);
            return ;
        }
        client_ptr++;
    }
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_server::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::enable_thread_safety");
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

int32_t game_server::disable_thread_safety() noexcept
{
    pt_recursive_mutex *old_mutex;
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::disable_thread_safety");
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

ft_bool game_server::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

void game_server::join_client_locked(int32_t client_id, int32_t client_handle) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::join_client_locked");
    this->_clients.insert(client_id, client_handle);
    if (this->_on_join != ft_nullptr)
        this->_on_join(client_id);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_server::leave_client_locked(int32_t client_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::leave_client_locked");
    this->_clients.remove(client_id);
    if (this->_on_leave != ft_nullptr)
        this->_on_leave(client_id);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}
