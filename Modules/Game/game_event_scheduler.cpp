#include "../PThread/pthread_internal.hpp"
#include "game_event_scheduler.hpp"
#include "../JSon/json.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/move.hpp"
#include <cstdio>
#include <charconv>
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
    : _events(), _pending_events(), _mutex(ft_nullptr),
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
    initialize_error = this->_pending_events.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(initialize_error));
    }
    initialize_error = this->_ready_cache.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_pending_events.destroy();
        (void)this->_events.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(initialize_error));
        return (initialize_error);
    }
    game_event_clear_queue(this->_events);
    this->_ready_cache.clear();
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
    int32_t pending_destroy_error;
    int32_t ready_cache_destroy_error;
    int32_t final_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    game_event_clear_queue(this->_events);
    game_event_clear_queue(this->_pending_events);
    this->_ready_cache.clear();
    events_destroy_error = this->_events.destroy();
    pending_destroy_error = this->_pending_events.destroy();
    ready_cache_destroy_error = this->_ready_cache.destroy();
    this->_profiling_enabled = FT_FALSE;
    event_scheduler_profile_reset_struct(this->_profile);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    final_error = disable_error;
    if (final_error == FT_ERR_SUCCESS && events_destroy_error != FT_ERR_SUCCESS)
        final_error = events_destroy_error;
    if (final_error == FT_ERR_SUCCESS && pending_destroy_error != FT_ERR_SUCCESS)
        final_error = pending_destroy_error;
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
    move_error = this->_pending_events.move(other._pending_events);
    if (move_error == FT_ERR_SUCCESS)
        move_error = this->_ready_cache.move(other._ready_cache);
    if (move_error != FT_ERR_SUCCESS)
    {
        game_event_clear_queue(this->_events);
        (void)this->_events.destroy();
        (void)this->_pending_events.destroy();
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

static ft_bool game_event_matches_id(const ft_sharedptr<game_event> *event_handle,
    int32_t id) noexcept
{
    return (event_handle != ft_nullptr && *event_handle
        && (*event_handle)->get_id() == id);
}

void game_event_scheduler::cancel_event(int32_t id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_event_scheduler::cancel_event");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const ft_bool erased = this->_events.erase_if(
        [id](ft_sharedptr<game_event> *event_handle) noexcept -> ft_bool
        {
            if (!game_event_matches_id(event_handle, id))
                return (FT_FALSE);
            game_event_delete_handle(event_handle);
            return (FT_TRUE);
        });
    int32_t queue_error = this->_events.get_error();
    if (!erased && queue_error == FT_ERR_SUCCESS)
        queue_error = FT_ERR_GAME_GENERAL_ERROR;
    this->set_error(queue_error);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_event_scheduler::reschedule_event(int32_t id, int32_t new_duration) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_event_scheduler::reschedule_event");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const ft_bool updated = this->_events.update_elements(
        [id, new_duration](ft_sharedptr<game_event> *event_handle) noexcept -> ft_bool
        {
            if (!game_event_matches_id(event_handle, id))
                return (FT_FALSE);
            (*event_handle)->set_duration(new_duration);
            return (FT_TRUE);
        });
    int32_t queue_error = this->_events.get_error();
    if (!updated && queue_error == FT_ERR_SUCCESS)
        queue_error = FT_ERR_GAME_GENERAL_ERROR;
    this->set_error(queue_error);
    this->unlock_internal(lock_acquired);
    return ;
}

static int32_t game_event_stage_ready(ft_sharedptr<game_event> *event_handle,
    ft_vector<ft_sharedptr<game_event> *> &ready_events,
    ft_vector<ft_sharedptr<game_event> > &ready_cache) noexcept
{
    int32_t result = ready_events.push_back(event_handle);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = ready_cache.push_back(*event_handle);
    if (result != FT_ERR_SUCCESS)
        ready_events.pop_back();
    return (result);
}

void game_event_scheduler::update_events(ft_sharedptr<game_world> &world,
        int32_t ticks, const char *log_file_path,
        ft_string *log_buffer) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::update_events");
    if (!world)
    {
        this->set_error(FT_ERR_GAME_GENERAL_ERROR);
        return ;
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return ;
    if (this->_events.empty())
    {
        this->_ready_cache.clear();
        this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    auto &pending = this->_pending_events;
    ft_vector<ft_sharedptr<game_event> *> ready_events;
    result = pending.reserve(this->_events.capacity());
    if (result == FT_ERR_SUCCESS)
        result = ready_events.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(result);
        return ;
    }

    this->_ready_cache.clear();
    ft_bool can_move_pending = FT_TRUE;
    int32_t previous_pending_duration = 0;
    while (!this->_events.empty())
    {
        ft_sharedptr<game_event> *current_event = this->_events.pop();
        game_event *event = ft_nullptr;
        if (current_event != ft_nullptr)
            event = current_event->get();
        if (event == ft_nullptr)
        {
            game_event_delete_handle(current_event);
            continue ;
        }
        int32_t remaining_duration = 0;
        event->subtract_duration(ticks, &remaining_duration);
        if (remaining_duration > 0)
        {
            if (remaining_duration <= previous_pending_duration)
                can_move_pending = FT_FALSE;
            previous_pending_duration = remaining_duration;
            pending.push(current_event);
            continue ;
        }

        // This handle may refer to a duration already used by the pending heap.
        if (previous_pending_duration > 0)
            can_move_pending = FT_FALSE;
        result = game_event_stage_ready(current_event, ready_events, this->_ready_cache);
        if (result != FT_ERR_SUCCESS)
        {
            this->_events.push(current_event);
            break ;
        }
        if (log_file_path)
            (void)log_event_to_file(*event, log_file_path);
        if (log_buffer)
            log_event_to_buffer(*event, *log_buffer);
    }

    // A ready event can share its duration with a handle already in pending.
    // Restore through the original heap for aliases, ties and failed staging.
    if (result == FT_ERR_SUCCESS && can_move_pending && !pending.empty())
    {
        // Rotate the two allocations so both queues retain storage for the next tick.
        ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> exhausted;
        result = exhausted.move(this->_events);
        if (result == FT_ERR_SUCCESS)
            result = this->_events.move(pending);
        if (result == FT_ERR_SUCCESS)
            result = pending.move(exhausted);
    }
    else
    {
        // The original queue still has capacity for every handle in this tick.
        while (!pending.empty())
            this->_events.push(pending.pop());
    }
    this->unlock_internal(lock_acquired);

    ft_size_t ready_event_index = 0U;
    const ft_size_t ready_count = ready_events.size();
    while (ready_event_index < ready_count)
    {
        ft_sharedptr<game_event> *current_event = ready_events[ready_event_index];
        game_event *event = current_event->get();
        const ft_function<void(game_world&, game_event&)> &callback = event->get_callback();
        game_world *world_pointer = world.get();
        if (callback && world_pointer != ft_nullptr)
            callback(*world_pointer, *event);
        game_event_delete_handle(current_event);
        ++ready_event_index;
    }
    this->set_error(result);
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
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event_scheduler::dump_events");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    out.clear();
    if (result == FT_ERR_SUCCESS)
        result = out.get_error();
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(result);
        return ;
    }
    const ft_size_t count = this->_events.size();
    result = this->_events.get_error();
    if (result == FT_ERR_SUCCESS)
    {
        out.reserve(count);
        result = out.get_error();
    }
    ft_priority_queue<ft_sharedptr<game_event> *, game_event_compare_ptr> queued_events(count);
    if (result == FT_ERR_SUCCESS)
        result = queued_events.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        this->set_error(result);
        return ;
    }

    // Both queues and the output already have capacity for every pending event.
    ft_size_t index = 0U;
    while (index < count)
    {
        ft_sharedptr<game_event> *event = this->_events.pop();
        result = this->_events.get_error();
        if (result != FT_ERR_SUCCESS)
            break ;
        queued_events.push(event);
        ++index;
    }
    while (index != 0U)
    {
        ft_sharedptr<game_event> *event = queued_events.pop();
        this->_events.push(event);
        if (result == FT_ERR_SUCCESS)
        {
            if (event == ft_nullptr)
                result = FT_ERR_INVALID_STATE;
            else
                result = out.push_back(*event);
        }
        --index;
    }
    if (result != FT_ERR_SUCCESS)
        out.clear();
    this->unlock_internal(lock_acquired);
    this->set_error(result);
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
    this->_ready_cache.clear();
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

static int32_t snapshot_saved_events(const ft_sharedptr<game_event_scheduler> &scheduler,
    ft_vector<ft_sharedptr<game_event> > &events, ft_size_t &count) noexcept
{
    if (!scheduler)
        return (FT_ERR_INVALID_POINTER);
    int32_t result = events.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    scheduler->dump_events(events);
    result = scheduler->get_error();
    if (result != FT_ERR_SUCCESS)
        return (result);
    count = events.size();
    if (count > static_cast<ft_size_t>(INT32_MAX))
        return (FT_ERR_OUT_OF_RANGE);
    return (events.get_error());
}

template <typename Emit>
static int32_t emit_saved_event_fields(const ft_vector<ft_sharedptr<game_event> > &events,
    ft_size_t count, Emit emit) noexcept
{
    int32_t result = emit("event_count", sizeof("event_count") - 1U,
        static_cast<int32_t>(count));
    const auto *events_data = events.begin();
    ft_size_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < count)
    {
        const game_event *event = events_data[index].get();
        if (event == ft_nullptr)
            return (FT_ERR_INVALID_POINTER);
        int32_t identifier;
        int32_t duration;
        result = event->get_schedule_values(identifier, duration);
        if (result != FT_ERR_SUCCESS)
            return (result);
        char key[32] = "event_";
        const auto number = std::to_chars(key + sizeof("event_") - 1U,
            key + sizeof(key) - sizeof("_duration"), static_cast<int32_t>(index));
        if (number.ec != std::errc())
            return (FT_ERR_OUT_OF_RANGE);
        const ft_size_t key_prefix_length = static_cast<ft_size_t>(number.ptr - key);
        ft_memcpy_fixed<sizeof("_id")>(number.ptr, "_id");
        result = emit(key, key_prefix_length + sizeof("_id") - 1U, identifier);
        if (result == FT_ERR_SUCCESS)
        {
            ft_memcpy_fixed<sizeof("_duration")>(number.ptr, "_duration");
            result = emit(key, key_prefix_length + sizeof("_duration") - 1U, duration);
        }
        ++index;
    }
    return (result);
}

json_group *serialize_event_scheduler(const ft_sharedptr<game_event_scheduler> &scheduler)
{
    ft_vector<ft_sharedptr<game_event> > events;
    ft_size_t count;
    if (snapshot_saved_events(scheduler, events, count) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    json_group *group = json_create_json_group("world");
    if (group == ft_nullptr)
        return (ft_nullptr);
    json_item **next_field = &group->items;
    const int32_t result = emit_saved_event_fields(events, count,
        [&next_field](const char *key, ft_size_t, int32_t value) noexcept
        {
            json_item *field = json_create_item(key, value);
            if (field == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
            *next_field = field;
            next_field = &field->next;
            return (FT_ERR_SUCCESS);
        });
    if (result != FT_ERR_SUCCESS)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    return (group);
}

template <ft_size_t Size>
static void write_event_json_fragment(char *&cursor, const char (&fragment)[Size]) noexcept
{
    ft_memcpy_fixed<Size - 1U>(cursor, fragment);
    cursor += Size - 1U;
    return ;
}

int32_t serialize_event_scheduler_buffer(const ft_sharedptr<game_event_scheduler> &scheduler,
    ft_string &output) noexcept
{
    constexpr char PREFIX[] = "{\n  \"world\": {\n";
    constexpr char SUFFIX[] = "\n  }\n}\n";
    constexpr ft_size_t MAX_FIELD_SIZE = sizeof(",\n    \"\": \"\"") - 1U
        + sizeof("event_2147483647_duration") - 1U + sizeof("-2147483648") - 1U;
    constexpr ft_size_t MAX_EVENT_SIZE = 2U * MAX_FIELD_SIZE;
    constexpr ft_size_t FIXED_SIZE = sizeof(PREFIX) - 1U + sizeof(SUFFIX) + MAX_FIELD_SIZE;
    ft_vector<ft_sharedptr<game_event> > events;
    ft_size_t count;
    int32_t result = snapshot_saved_events(scheduler, events, count);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (count > (FT_SYSTEM_SIZE_MAX - FIXED_SIZE) / MAX_EVENT_SIZE)
        return (FT_ERR_OUT_OF_RANGE);
    // The upper bound includes two fields per event and the final terminator.
    char *buffer = static_cast<char *>(cma_malloc(FIXED_SIZE + count * MAX_EVENT_SIZE));
    if (buffer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    char *cursor = buffer;
    write_event_json_fragment(cursor, PREFIX);
    ft_bool first_field = FT_TRUE;
    result = emit_saved_event_fields(events, count,
        [&cursor, &first_field](const char *key, ft_size_t key_length, int32_t value) noexcept
        {
            if (!first_field)
                write_event_json_fragment(cursor, ",\n");
            first_field = FT_FALSE;
            write_event_json_fragment(cursor, "    \"");
            ft_memcpy(cursor, key, key_length);
            cursor += key_length;
            write_event_json_fragment(cursor, "\": \"");
            const auto number = std::to_chars(cursor, cursor + sizeof("-2147483648") - 1U, value);
            if (number.ec != std::errc())
                return (FT_ERR_OUT_OF_RANGE);
            cursor = number.ptr;
            write_event_json_fragment(cursor, "\"");
            return (FT_ERR_SUCCESS);
        });
    if (result == FT_ERR_SUCCESS)
    {
        write_event_json_fragment(cursor, SUFFIX);
        *cursor = '\0';
        const ft_size_t length = static_cast<ft_size_t>(cursor - buffer);
        if (output.is_initialised() && output.size() >= length)
            result = output.assign(buffer, length);
        else
        {
            result = ft_string_take_owned_buffer(output, buffer, length);
            if (result == FT_ERR_SUCCESS)
                return (FT_ERR_SUCCESS);
        }
    }
    cma_free(buffer);
    return (result);
}


static int32_t read_saved_event(json_group *group, const json_item *&cursor,
    int32_t index, int32_t &id, int32_t &duration)
{
    char key[32] = "event_";
    const auto number = std::to_chars(key + sizeof("event_") - 1U,
        key + sizeof(key) - sizeof("_duration"), index);
    if (number.ec != std::errc())
        return (FT_ERR_OUT_OF_RANGE);

    if (cursor != ft_nullptr)
    {
        const int32_t error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
        if (error != FT_ERR_SUCCESS)
            return (error);
        ft_memcpy(number.ptr, "_id", sizeof("_id"));
        const ft_bool id_matches = cursor->key != ft_nullptr && ft_strcmp(cursor->key, key) == 0;
        const json_item *duration_field = cursor->next;
        ft_memcpy(number.ptr, "_duration", sizeof("_duration"));
        if (id_matches == FT_TRUE && duration_field != ft_nullptr
            && duration_field->key != ft_nullptr && ft_strcmp(duration_field->key, key) == 0)
        {
            id = ft_atoi(cursor->value);
            duration = ft_atoi(duration_field->value);
            cursor = duration_field->next;
            (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
            return (FT_ERR_SUCCESS);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        cursor = ft_nullptr;
    }

    ft_memcpy(number.ptr, "_id", sizeof("_id"));
    const json_item *id_field = json_find_item(group, key);
    ft_memcpy(number.ptr, "_duration", sizeof("_duration"));
    const json_item *duration_field = json_find_item(group, key);
    if (id_field == ft_nullptr || duration_field == ft_nullptr)
        return (FT_ERR_GAME_GENERAL_ERROR);
    id = ft_atoi(id_field->value);
    duration = ft_atoi(duration_field->value);
    return (FT_ERR_SUCCESS);
}

int32_t deserialize_event_scheduler(ft_sharedptr<game_event_scheduler> &scheduler, json_group *group)
{
    if (!scheduler)
        return (FT_ERR_GAME_GENERAL_ERROR);
    const json_item *count_item = json_find_item(group, "event_count");
    if (count_item == ft_nullptr)
        return (FT_ERR_GAME_GENERAL_ERROR);
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    // Only follow the writer's order while every field matches from the head.
    const json_item *cursor = ft_nullptr;
    if (count_item == group->items)
        cursor = count_item->next;
    const int32_t event_count = ft_atoi(count_item->value);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    int32_t event_index = 0;
    while (event_index < event_count)
    {
        int32_t id;
        int32_t duration;
        int32_t result = read_saved_event(group, cursor, event_index, id, duration);
        if (result != FT_ERR_SUCCESS)
            return (result);
        ft_sharedptr<game_event> event(new (std::nothrow) game_event());
        if (!event)
            return (FT_ERR_NO_MEMORY);
        if (event->initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        event->set_id(id);
        event->set_duration(duration);
        scheduler->schedule_event(event);
        result = scheduler->get_error();
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++event_index;
    }
    return (FT_ERR_SUCCESS);
}
