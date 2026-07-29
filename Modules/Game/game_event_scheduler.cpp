#include "../PThread/pthread_internal.hpp"
#include "game_event_scheduler.hpp"
#include "../JSon/json.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/move.hpp"
#include <cstdio>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/priority_queue.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include "game_event.hpp"

thread_local int32_t game_event_scheduler::_last_error = FT_ERR_SUCCESS;
static void event_scheduler_profile_reset_struct(t_event_scheduler_profile &profile)
{
    profile.update_count = 0;
    profile.events_processed = 0;
    profile.events_rescheduled = 0;
    profile.max_queue_depth = 0;
    profile.max_ready_batch = 0;
    profile.total_processing_ns = 0;
    profile.last_update_processing_ns = 0;
    profile.last_error_code = FT_ERR_SUCCESS;
    return ;
}

static void game_event_delete_handle(ft_sharedptr<game_event> *event_handle) noexcept
{
    if (event_handle == ft_nullptr)
        return ;
    (void)event_handle->destroy();
    delete event_handle;
    return ;
}

static ft_sharedptr<game_event> *game_event_clone_handle(
    const ft_sharedptr<game_event> &event_handle) noexcept
{
    ft_sharedptr<game_event> *clone;

    clone = new (std::nothrow) ft_sharedptr<game_event>();
    if (clone == ft_nullptr)
        return (ft_nullptr);
    if (clone->initialize() != FT_ERR_SUCCESS)
    {
        delete clone;
        return (ft_nullptr);
    }
    *clone = event_handle;
    if (clone->get_error() != FT_ERR_SUCCESS)
    {
        game_event_delete_handle(clone);
        return (ft_nullptr);
    }
    return (clone);
}

static void game_event_clear_handle_vector(
    ft_vector<ft_sharedptr<game_event> *> &events) noexcept
{
    ft_size_t event_index;

    event_index = 0;
    while (event_index < events.size())
    {
        game_event_delete_handle(events[event_index]);
        event_index++;
    }
    events.clear();
    return ;
}

static void game_event_clear_queue(
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> &events) noexcept
{
    ft_sharedptr<game_event> *event_handle;

    while (!events.empty())
    {
        event_handle = events.pop();
        if (events.get_error() == FT_ERR_SUCCESS)
            game_event_delete_handle(event_handle);
    }
    return ;
}

void game_event_scheduler::reset_profile_locked() const noexcept
{
    event_scheduler_profile_reset_struct(this->_profile);
    this->_profile.last_error_code = this->get_error();
    return ;
}

void game_event_scheduler::record_profile_locked(ft_size_t ready_count,
                ft_size_t rescheduled_count,
                ft_size_t queue_depth,
                int64_t duration_ns) const noexcept
{
    this->_profile.update_count += 1;
    this->_profile.events_processed += static_cast<int64_t>(ready_count);
    this->_profile.events_rescheduled += static_cast<int64_t>(rescheduled_count);
    if (queue_depth > this->_profile.max_queue_depth)
        this->_profile.max_queue_depth = queue_depth;
    if (ready_count > this->_profile.max_ready_batch)
        this->_profile.max_ready_batch = ready_count;
    this->_profile.total_processing_ns += duration_ns;
    this->_profile.last_update_processing_ns = duration_ns;
    this->_profile.last_error_code = this->get_error();
    return ;
}

void game_event_scheduler::finalize_update(ft_vector<ft_sharedptr<game_event> *> &events,
                ft_size_t ready_count,
                ft_size_t rescheduled_count,
                ft_size_t queue_depth,
                ft_bool profiling_active,
                ft_bool,
                t_high_resolution_time_point) noexcept
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
    game_event_clear_handle_vector(this->_ready_cache);
    while (events.size() > 0)
    {
        this->_ready_cache.push_back(events[events.size() - 1]);
        events.pop_back();
    }
    if (profiling_active && this->_profiling_enabled)
        this->record_profile_locked(ready_count, rescheduled_count, queue_depth, 0);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_bool game_event_compare_ptr::operator()(const ft_sharedptr<game_event> *left,
        const ft_sharedptr<game_event> *right) const noexcept
{
    const game_event *left_pointer;
    const game_event *right_pointer;

    if (left == ft_nullptr)
        return (FT_FALSE);
    if (right == ft_nullptr)
        return (FT_TRUE);
    left_pointer = left->get();
    right_pointer = right->get();
    if (left_pointer == ft_nullptr)
        return (FT_FALSE);
    if (right_pointer == ft_nullptr)
        return (FT_TRUE);
    return (left_pointer->get_duration() > right_pointer->get_duration());
}

game_event_scheduler::game_event_scheduler() noexcept
    : _events(), _mutex(ft_nullptr),
      _profiling_enabled(FT_FALSE), _profile(), _ready_cache(),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    event_scheduler_profile_reset_struct(this->_profile);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_event_scheduler::~game_event_scheduler() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_event_scheduler::set_error(int32_t error_code) noexcept
{
    game_event_scheduler::_last_error = error_code;
    return (error_code);
}

int32_t game_event_scheduler::initialize() noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_event_scheduler::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->_events.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(initialize_error));
        return (initialize_error);
    }
    initialize_error = this->_ready_cache.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(initialize_error));
        return (initialize_error);
    }
    game_event_clear_queue(this->_events);
    game_event_clear_handle_vector(this->_ready_cache);
    this->_profiling_enabled = FT_FALSE;
    event_scheduler_profile_reset_struct(this->_profile);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_event_scheduler::destroy() noexcept
{
    int32_t disable_error;
    int32_t events_destroy_error;
    int32_t ready_cache_destroy_error;
    int32_t final_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    game_event_clear_queue(this->_events);
    game_event_clear_handle_vector(this->_ready_cache);
    events_destroy_error = this->_events.destroy();
    ready_cache_destroy_error = this->_ready_cache.destroy();
    this->_profiling_enabled = FT_FALSE;
    event_scheduler_profile_reset_struct(this->_profile);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    final_error = disable_error;
    if (final_error == FT_ERR_SUCCESS && events_destroy_error != FT_ERR_SUCCESS)
        final_error = events_destroy_error;
    if (final_error == FT_ERR_SUCCESS && ready_cache_destroy_error != FT_ERR_SUCCESS)
        final_error = ready_cache_destroy_error;
    this->set_error(final_error);
    return (final_error);
}

int32_t game_event_scheduler::move(game_event_scheduler &other) noexcept
{
    int32_t destroy_error;
    int32_t move_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_event_scheduler::move", "source object is uninitialised");
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
    move_error = this->_events.move(other._events);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    move_error = this->_ready_cache.move(other._ready_cache);
    if (move_error != FT_ERR_SUCCESS)
    {
        game_event_clear_queue(this->_events);
        (void)this->_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(move_error);
        return (move_error);
    }
    this->_profiling_enabled = other._profiling_enabled;
    this->_profile = other._profile;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._profiling_enabled = FT_FALSE;
    event_scheduler_profile_reset_struct(other._profile);
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

int32_t game_event_scheduler::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::lock_internal");
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

void game_event_scheduler::unlock_internal(ft_bool lock_acquired) const noexcept
{

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::unlock_internal");
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void game_event_scheduler::schedule_event(const ft_sharedptr<game_event> &event) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_sharedptr<game_event> *event_handle;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::schedule_event");
    if (!event)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    event_handle = game_event_clone_handle(event);
    if (event_handle == ft_nullptr)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_NO_MEMORY);
        return ;
    }
    this->_events.push(event_handle);
    if (this->_events.get_error() != FT_ERR_SUCCESS)
    {
        game_event_delete_handle(event_handle);
        this->unlock_internal(lock_acquired);
        this->set_error(this->_events.get_error());
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::cancel_event(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t queue_error;
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> temporary_queue;
    ft_sharedptr<game_event> *current_event;
    ft_bool event_found;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::cancel_event");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    queue_error = temporary_queue.initialize();
    if (queue_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(temporary_queue.get_error());
        return ;
    }
    event_found = FT_FALSE;
    while (!this->_events.empty())
    {
        current_event = this->_events.pop();
        if (current_event != ft_nullptr && *current_event
            && (*current_event)->get_id() == id)
        {
            event_found = FT_TRUE;
            game_event_delete_handle(current_event);
        }
        else
            temporary_queue.push(current_event);
    }
    while (!temporary_queue.empty())
        this->_events.push(temporary_queue.pop());
    if (event_found)
        this->set_error(FT_ERR_SUCCESS);
    else
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::reschedule_event(int32_t id, int32_t new_duration) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t queue_error;
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> temporary_queue;
    ft_sharedptr<game_event> *current_event;
    ft_bool event_found;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::reschedule_event");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    queue_error = temporary_queue.initialize();
    if (queue_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(temporary_queue.get_error());
        return ;
    }
    event_found = FT_FALSE;
    while (!this->_events.empty())
    {
        current_event = this->_events.pop();
        if (current_event != ft_nullptr && *current_event
            && (*current_event)->get_id() == id)
        {
            (*current_event)->set_duration(new_duration);
            event_found = FT_TRUE;
        }
        temporary_queue.push(current_event);
    }
    while (!temporary_queue.empty())
        this->_events.push(temporary_queue.pop());
    if (event_found)
        this->set_error(FT_ERR_SUCCESS);
    else
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::update_events(ft_sharedptr<game_world> &world,
        int32_t ticks, const char *log_file_path,
        ft_string *log_buffer) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t queue_error;
    int32_t ready_events_error;
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> temporary_queue;
    ft_vector<ft_sharedptr<game_event> *> ready_events;
    ft_sharedptr<game_event> *current_event;
    ft_size_t ready_event_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::update_events");
    if (!world)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    queue_error = temporary_queue.initialize();
    if (queue_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(temporary_queue.get_error());
        return ;
    }
    ready_events_error = ready_events.initialize();
    if (ready_events_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(ready_events.get_error());
        return ;
    }
    this->_ready_cache.clear();
    while (!this->_events.empty())
    {
        current_event = this->_events.pop();
        if (current_event == ft_nullptr || !*current_event)
        {
            game_event_delete_handle(current_event);
            continue ;
        }
        (*current_event)->sub_duration(ticks);
        if ((*current_event)->get_duration() <= 0)
        {
            if (log_file_path)
                (void)log_event_to_file(**current_event, log_file_path);
            if (log_buffer)
                log_event_to_buffer(**current_event, *log_buffer);
            this->_ready_cache.push_back(game_event_clone_handle(*current_event));
            if (this->_ready_cache.get_error() != FT_ERR_SUCCESS)
            {
                game_event_delete_handle(current_event);
                this->unlock_internal(lock_acquired);
                this->set_error(this->_ready_cache.get_error());
                return ;
            }
            ready_events.push_back(current_event);
            if (ready_events.get_error() != FT_ERR_SUCCESS)
            {
                game_event_delete_handle(current_event);
                this->unlock_internal(lock_acquired);
                this->set_error(ready_events.get_error());
                return ;
            }
        }
        else
            temporary_queue.push(current_event);
    }
    while (!temporary_queue.empty())
        this->_events.push(temporary_queue.pop());
    this->unlock_internal(lock_acquired);
    ready_event_index = 0;
    while (ready_event_index < ready_events.size())
    {
        current_event = ready_events[ready_event_index];
        if (current_event != ft_nullptr && *current_event)
        {
            game_world *world_pointer;
            const ft_function<void(game_world&, game_event&)> &callback = (*current_event)->get_callback();

            world_pointer = world.get();
            if (callback && world_pointer != ft_nullptr)
                callback(*world_pointer, **current_event);
        }
        game_event_delete_handle(current_event);
        ready_event_index++;
    }
    ready_events.clear();
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_event_scheduler::enable_profiling(ft_bool enabled) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::enable_profiling");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_profiling_enabled = enabled;
    this->reset_profile_locked();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_bool game_event_scheduler::profiling_enabled() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool enabled;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::profiling_enabled");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    enabled = this->_profiling_enabled;
    this->unlock_internal(lock_acquired);
    return (enabled);
}

void game_event_scheduler::reset_profile() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::reset_profile");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->reset_profile_locked();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::snapshot_profile(t_event_scheduler_profile &out) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::snapshot_profile");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    out = this->_profile;
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::dump_events(ft_vector<ft_sharedptr<game_event> > &out) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> queued_events;
    ft_sharedptr<game_event> *current_event;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::dump_events");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        out.clear();
        return ;
    }
    out.clear();
    if (queued_events.initialize() != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    while (!this->_events.empty())
    {
        current_event = this->_events.pop();
        if (this->_events.get_error() == FT_ERR_SUCCESS)
            queued_events.push(current_event);
    }
    while (!queued_events.empty())
    {
        current_event = queued_events.pop();
        if (queued_events.get_error() == FT_ERR_SUCCESS)
        {
            this->_events.push(current_event);
            out.push_back(*current_event);
        }
    }
    this->unlock_internal(lock_acquired);
    return ;
}

ft_size_t game_event_scheduler::size() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t queue_size;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::size");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    queue_size = this->_events.size();
    this->unlock_internal(lock_acquired);
    return (queue_size);
}

void game_event_scheduler::clear() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::clear");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    game_event_clear_queue(this->_events);
    game_event_clear_handle_vector(this->_ready_cache);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_event_scheduler::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_event_scheduler::get_error");
    return (game_event_scheduler::_last_error);
}

const char *game_event_scheduler::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_event_scheduler::get_error_str");
    return (ft_strerror(game_event_scheduler::_last_error));
}

int32_t game_event_scheduler::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::enable_thread_safety");
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

int32_t game_event_scheduler::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_event_scheduler::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

int32_t log_event_to_file(const game_event &event, const char *file_path) noexcept
{
    FILE *file;

    file = fopen(file_path, "a");
    if (!file)
        return (FT_ERR_GAME_GENERAL_ERROR);
    if (fprintf(file, "event %d processed\n", event.get_id()) < 0)
    {
        fclose(file);
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    fclose(file);
    return (FT_ERR_SUCCESS);
}

void log_event_to_buffer(const game_event &event, ft_string &buffer) noexcept
{
    char number_buffer[32];

    if (pf_snprintf(number_buffer, sizeof(number_buffer), "%d", event.get_id()) < 0)
        return ;
    buffer += "event ";
    buffer += number_buffer;
    buffer += " processed\n";
    return ;
}

static int32_t add_item_field(json_group *group, const ft_string &key, int32_t value)
{
    json_item *json_item_ptr;

    json_item_ptr = json_create_item(key.c_str(), value);
    if (!json_item_ptr)
    {
        json_free_groups(group);
        return (FT_ERR_NO_MEMORY);
    }
    json_add_item_to_group(group, json_item_ptr);
    return (FT_ERR_SUCCESS);
}

json_group *serialize_event_scheduler(const ft_sharedptr<game_event_scheduler> &scheduler)
{
    json_group *group;
    json_item *count_item;
    ft_vector<ft_sharedptr<game_event> > events;
    ft_size_t event_index;
    ft_size_t event_count;

    if (!scheduler)
        return (ft_nullptr);
    if (events.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    group = json_create_json_group("world");
    if (!group)
        return (ft_nullptr);
    count_item = json_create_item("event_count", static_cast<int32_t>(scheduler->size()));
    if (!count_item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, count_item);
    scheduler->dump_events(events);
    event_index = 0;
    event_count = events.size();
    while (event_index < event_count)
    {
        char event_index_buffer[32];
        ft_string key_id;
        ft_string key_duration;

        if (pf_snprintf(event_index_buffer, sizeof(event_index_buffer), "%d", static_cast<int32_t>(event_index)) < 0)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        key_id = "event_";
        key_id += event_index_buffer;
        key_id += "_id";
        key_duration = "event_";
        key_duration += event_index_buffer;
        key_duration += "_duration";
        if (!events[event_index]
            || add_item_field(group, key_id, events[event_index]->get_id()) != FT_ERR_SUCCESS
            || add_item_field(group, key_duration, events[event_index]->get_duration()) != FT_ERR_SUCCESS)
        {
            events.clear();
            return (ft_nullptr);
        }
        event_index += 1;
    }
    events.clear();
    return (group);
}

int32_t deserialize_event_scheduler(ft_sharedptr<game_event_scheduler> &scheduler, json_group *group)
{
    json_item *count_item;
    int32_t event_count;
    int32_t event_index;

    if (!scheduler)
        return (FT_ERR_GAME_GENERAL_ERROR);
    count_item = json_find_item(group, "event_count");
    if (!count_item)
        return (FT_ERR_GAME_GENERAL_ERROR);
    event_count = ft_atoi(count_item->value);
    event_index = 0;
    while (event_index < event_count)
    {
        char event_index_buffer[32];
        ft_string key_id;
        ft_string key_duration;
        json_item *id_item;
        json_item *duration_item;
        ft_sharedptr<game_event> event;

        if (pf_snprintf(event_index_buffer, sizeof(event_index_buffer), "%d", event_index) < 0)
            return (FT_ERR_NO_MEMORY);
        key_id = "event_";
        key_id += event_index_buffer;
        key_id += "_id";
        key_duration = "event_";
        key_duration += event_index_buffer;
        key_duration += "_duration";
        id_item = json_find_item(group, key_id.c_str());
        duration_item = json_find_item(group, key_duration.c_str());
        if (!id_item || !duration_item)
            return (FT_ERR_GAME_GENERAL_ERROR);
        event = ft_sharedptr<game_event>(new (std::nothrow) game_event());
        if (!event)
            return (FT_ERR_NO_MEMORY);
        if (event->initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        event->set_id(ft_atoi(id_item->value));
        event->set_duration(ft_atoi(duration_item->value));
        scheduler->schedule_event(event);
        event_index += 1;
    }
    return (FT_ERR_SUCCESS);
}
