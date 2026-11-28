#include "../PThread/pthread_internal.hpp"
#include "game_server.hpp"
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
      _on_join(ft_nullptr), _on_leave(ft_nullptr), _on_message(ft_nullptr),
      _on_parsed_message(ft_nullptr),
      _message_user_data(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_server::initialize() noexcept
{
    ft_sharedptr<game_world> world;
    const int32_t result = world.initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    return (this->initialize(world, ft_nullptr));
}

int32_t game_server::initialize(const ft_sharedptr<game_world> &world,
    const char *auth_token) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_server::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    int32_t result = this->_world.initialize(world);
    if (result == FT_ERR_SUCCESS)
        result = this->_auth_token.initialize(auth_token);
    if (result == FT_ERR_SUCCESS)
        result = this->_clients.initialize();
    if (result == FT_ERR_SUCCESS)
    {
        this->_server = new (std::nothrow) ft_websocket_server();
        if (this->_server == ft_nullptr)
            result = FT_ERR_NO_MEMORY;
        else
            result = this->_server->initialize();
    }
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(result));
}

int32_t game_server::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    if (this->_server != ft_nullptr)
    {
        const int32_t transport_error = this->_server->destroy();
        if (result == FT_ERR_SUCCESS)
            result = transport_error;
        delete this->_server;
        this->_server = ft_nullptr;
    }
    const int32_t world_error = this->_world.destroy();
    const int32_t clients_error = this->_clients.destroy();
    const int32_t token_error = this->_auth_token.destroy();
    this->_on_join = ft_nullptr;
    this->_on_leave = ft_nullptr;
    this->_on_message = ft_nullptr;
    this->_on_parsed_message = ft_nullptr;
    this->_message_user_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (result == FT_ERR_SUCCESS)
        result = world_error;
    if (result == FT_ERR_SUCCESS)
        result = clients_error;
    if (result == FT_ERR_SUCCESS)
        result = token_error;
    return (this->set_error(result));
}

ft_bool game_server::has_active_lock() const noexcept
{
    return ((this->_mutex != ft_nullptr && this->_mutex->lockState())
        || this->_clients.has_active_lock());
}

int32_t game_server::move(game_server &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_server::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->has_active_lock())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    if (other.has_active_lock())
        return (this->set_error(FT_ERR_THREAD_BUSY));

    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = this->_clients.move(other._clients);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_auth_token.move(other._auth_token);
        if (result == FT_ERR_SUCCESS)
        {
            result = this->_world.move(other._world);
            if (result != FT_ERR_SUCCESS)
                (void)other._auth_token.move(this->_auth_token);
        }
        if (result != FT_ERR_SUCCESS)
            (void)other._clients.move(this->_clients);
    }
    if (result == FT_ERR_SUCCESS)
    {
        this->_server = other._server;
        this->_mutex = other._mutex;
        this->_on_join = other._on_join;
        this->_on_leave = other._on_leave;
        this->_on_message = other._on_message;
        this->_on_parsed_message = other._on_parsed_message;
        this->_message_user_data = other._message_user_data;
        other._server = ft_nullptr;
        other._mutex = ft_nullptr;
        result = other.destroy();
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
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

void game_server::set_message_callback(game_server_message_callback callback,
    void *user_data) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_server::set_message_callback");
    this->configure_message_callbacks(callback, ft_nullptr, user_data);
    return ;
}

void game_server::set_parsed_message_callback(game_server_parsed_message_callback callback,
    void *user_data) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_server::set_parsed_message_callback");
    this->configure_message_callbacks(ft_nullptr, callback, user_data);
    return ;
}

void game_server::configure_message_callbacks(game_server_message_callback raw_callback,
    game_server_parsed_message_callback parsed_callback, void *user_data) noexcept
{
    ft_bool lock_acquired;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_on_message = raw_callback;
    this->_on_parsed_message = parsed_callback;
    this->_message_user_data = user_data;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_server::send_to_client(int32_t client_handle,
    const ft_string &message) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t send_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_server::send_to_client");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    if (this->_server == ft_nullptr)
        send_error = FT_ERR_INVALID_STATE;
    else
        send_error = this->_server->send_text(client_handle, message);
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(send_error));
}

enum class game_server_request_type
{
    UNKNOWN,
    EVENT,
    LEAVE,
    JOIN,
    WORLD_REVISION
};

struct game_server_request
{
    game_server_request_type type;
    int32_t identifier;
    int32_t duration;
    ft_bool close_on_error;
    json_group *revision;
};

static json_group *select_request_group(json_group *groups,
    ft_bool has_message_callback, game_server_request_type &type) noexcept
{
    json_group *selected = ft_nullptr;
    while (groups != ft_nullptr)
    {
        const char *name = groups->name;
        if (name != ft_nullptr)
        {
            if (has_message_callback && name[0] == 'w' && ft_strcmp(name, "world_revision") == 0)
            {
                type = game_server_request_type::WORLD_REVISION;
                return (groups);
            }
            if (type < game_server_request_type::JOIN && name[0] == 'j' && ft_strcmp(name, "join") == 0)
            {
                type = game_server_request_type::JOIN;
                selected = groups;
                if (!has_message_callback)
                    break ;
            }
            else if (type < game_server_request_type::LEAVE && name[0] == 'l' && ft_strcmp(name, "leave") == 0)
            {
                type = game_server_request_type::LEAVE;
                selected = groups;
            }
            else if (type == game_server_request_type::UNKNOWN && name[0] == 'e' && ft_strcmp(name, "event") == 0)
            {
                type = game_server_request_type::EVENT;
                selected = groups;
            }
        }
        groups = groups->next;
    }
    return (selected);
}

static int32_t read_game_server_request(json_group *groups, const ft_string &auth_token,
    ft_bool has_message_callback, game_server_request &request) noexcept
{
    json_group *selected = select_request_group(groups, has_message_callback, request.type);
    if (selected == ft_nullptr)
        return (FT_ERR_GAME_GENERAL_ERROR);
    if (request.type == game_server_request_type::WORLD_REVISION)
    {
        request.revision = selected;
        return (FT_ERR_SUCCESS);
    }

    const ft_bool needs_token = request.type == game_server_request_type::JOIN
        && auth_token.size() != 0U;
    const ft_bool needs_duration = request.type == game_server_request_type::EVENT;
    const json_item *identifier = ft_nullptr;
    const json_item *token = ft_nullptr;
    const json_item *duration = ft_nullptr;
    const json_item *item = selected->items;
    while (item != ft_nullptr)
    {
        const char *key = item->key;
        if (key != ft_nullptr)
        {
            if (identifier == ft_nullptr && key[0] == 'i' && ft_strcmp(key, "id") == 0)
                identifier = item;
            else if (needs_token && token == ft_nullptr && key[0] == 't' && ft_strcmp(key, "token") == 0)
                token = item;
            else if (needs_duration && duration == ft_nullptr && key[0] == 'd' && ft_strcmp(key, "duration") == 0)
                duration = item;
        }
        if (identifier != ft_nullptr && (!needs_token || token != ft_nullptr)
            && (!needs_duration || duration != ft_nullptr))
            break ;
        item = item->next;
    }
    if (identifier == ft_nullptr || (needs_duration && duration == ft_nullptr))
        return (FT_ERR_GAME_GENERAL_ERROR);
    if (needs_token && (token == ft_nullptr || auth_token != token->value))
    {
        request.close_on_error = FT_TRUE;
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    request.identifier = ft_atoi(identifier->value);
    if (needs_duration)
        request.duration = ft_atoi(duration->value);
    return (FT_ERR_SUCCESS);
}

int32_t game_server::handle_message_locked(int32_t client_handle,
    const ft_string &message) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::handle_message_locked");
    const char *message_text = message.c_str();
    json_group *groups = json_read_from_string(message_text);
    if (groups == ft_nullptr)
        return (this->set_error(FT_ERR_GAME_GENERAL_ERROR));
    game_server_request request = {game_server_request_type::UNKNOWN, 0, 0, FT_FALSE, ft_nullptr};
    int32_t result = read_game_server_request(groups, this->_auth_token,
        this->_on_message != ft_nullptr || this->_on_parsed_message != ft_nullptr, request);
    if (result == FT_ERR_SUCCESS && request.type == game_server_request_type::WORLD_REVISION
        && this->_on_parsed_message != ft_nullptr)
    {
        result = this->_on_parsed_message(client_handle, message_text,
            *request.revision, this->_message_user_data);
        json_free_groups(groups);
        return (this->set_error(result));
    }
    json_free_groups(groups);
    if (result != FT_ERR_SUCCESS)
    {
        if (request.close_on_error)
            nw_close(client_handle);
        return (this->set_error(result));
    }
    if (request.type == game_server_request_type::WORLD_REVISION)
        result = this->_on_message(client_handle, message_text, this->_message_user_data);
    else if (request.type == game_server_request_type::JOIN)
        result = this->join_client_locked(request.identifier, client_handle);
    else if (request.type == game_server_request_type::LEAVE)
    {
        result = this->leave_client_locked(request.identifier);
        nw_close(client_handle);
    }
    else
        result = this->schedule_event_locked(request.identifier, request.duration);
    return (this->set_error(result));
}

int32_t game_server::schedule_event_locked(int32_t event_id, int32_t duration) noexcept
{
    if (event_id < 0 || duration < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (!this->_world)
        return (FT_ERR_GAME_GENERAL_ERROR);
    auto *event = new (std::nothrow) game_event();
    if (event == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_sharedptr<game_event> owner;
    int32_t result = owner.initialize(event);
    if (result != FT_ERR_SUCCESS)
    {
        delete event;
        return (result);
    }
    result = event->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    event->set_id(event_id);
    event->set_duration(duration);
    this->_world->schedule_event(owner);
    return (this->_world->get_error());
}


int32_t game_server::serialize_world_locked(ft_string &out) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::serialize_world_locked");
    if (!this->_world)
        return (this->set_error(FT_ERR_GAME_GENERAL_ERROR));
    ft_sharedptr<game_event_scheduler> scheduler;
    scheduler = this->_world->get_event_scheduler();
    if (!scheduler)
        return (this->set_error(FT_ERR_GAME_GENERAL_ERROR));
    return (this->set_error(serialize_event_scheduler_buffer(scheduler, out)));
}

int32_t game_server::broadcast_world_locked() noexcept
{
    const ft_size_t count = this->_clients.size();
    int32_t result = this->_clients.get_error();
    if (result != FT_ERR_SUCCESS || count == 0U)
        return (result);
    ft_string update;
    result = this->serialize_world_locked(update);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const auto *end = this->_clients.end();
    result = this->_clients.get_error();
    if (result != FT_ERR_SUCCESS)
        return (result);
    const auto *client = end - count;
    while (client != end)
    {
        constexpr ft_size_t BATCH_SIZE = 64U;
        int32_t descriptors[BATCH_SIZE];
        ft_size_t descriptor_count = 0U;
        while (client != end && descriptor_count < BATCH_SIZE)
        {
            descriptors[descriptor_count++] = client->value;
            ++client;
        }
        result = this->_server->send_text_many(descriptors, descriptor_count, update);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    return (FT_ERR_SUCCESS);
}

void game_server::run_once() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::run_once");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        if (this->_server == ft_nullptr)
            result = FT_ERR_INVALID_STATE;
        else
        {
            ft_string message;
            result = message.initialize();
            if (result == FT_ERR_SUCCESS)
            {
                int32_t client_handle;
                if (this->_server->run_once(client_handle, message) != FT_ERR_SUCCESS
                    || this->handle_message_locked(client_handle, message) != FT_ERR_SUCCESS)
                    result = FT_ERR_GAME_GENERAL_ERROR;
                else
                    result = this->broadcast_world_locked();
            }
        }
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
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

int32_t game_server::join_client_locked(int32_t client_id, int32_t client_handle) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::join_client_locked");
    this->_clients.insert(client_id, client_handle);
    const int32_t result = this->_clients.get_error();
    if (result == FT_ERR_SUCCESS && this->_on_join != ft_nullptr)
        this->_on_join(client_id);
    return (this->set_error(result));
}

int32_t game_server::leave_client_locked(int32_t client_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_server::leave_client_locked");
    this->_clients.remove(client_id);
    const int32_t result = this->_clients.get_error();
    if (result == FT_ERR_SUCCESS && this->_on_leave != ft_nullptr)
        this->_on_leave(client_id);
    return (this->set_error(result));
}
