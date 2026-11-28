#include "../PThread/pthread_internal.hpp"
#include "game_dialogue_table.hpp"
#include "../Printf/printf.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_dialogue_line.hpp"
#include "game_dialogue_script.hpp"

thread_local int32_t game_dialogue_table::_last_error = FT_ERR_SUCCESS;

int32_t game_dialogue_table::clone_line(const game_dialogue_line &line,
    game_dialogue_line *&clone) noexcept
{
    if (!line.is_initialised())
        return (FT_ERR_INVALID_STATE);
    game_dialogue_line *staged = new (std::nothrow) game_dialogue_line();
    if (staged == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t result = staged->initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged->copy_values(line);
    if (result == FT_ERR_SUCCESS)
        clone = staged;
    else
        delete staged;
    return (result);
}

static void game_dialogue_table_clear_line_cache(
    ft_map<int32_t, game_dialogue_line *> &line_cache) noexcept
{
    (void)line_cache.for_each([](const int32_t &, game_dialogue_line *&line) noexcept
    {
        delete line;
        line = ft_nullptr;
        return ;
    });
    line_cache.clear();
    return ;
}

game_dialogue_table::game_dialogue_table() noexcept
    : _lines(), _line_cache(), _scripts(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_dialogue_table::~game_dialogue_table() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_dialogue_table::set_error(int32_t error_code) noexcept
{
    game_dialogue_table::_last_error = error_code;
    return (error_code);
}

int32_t game_dialogue_table::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_table::get_error");
    return (game_dialogue_table::_last_error);
}

const char *game_dialogue_table::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_table::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_dialogue_table::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_dialogue_table::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t lines_error = this->_lines.initialize();
    if (lines_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(lines_error);
        return (lines_error);
    }
    int32_t line_cache_error = this->_line_cache.initialize();
    if (line_cache_error != FT_ERR_SUCCESS)
    {
        (void)this->_lines.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(line_cache_error);
        return (line_cache_error);
    }
    int32_t scripts_error = this->_scripts.initialize();
    if (scripts_error != FT_ERR_SUCCESS)
    {
        (void)this->_lines.destroy();
        (void)this->_line_cache.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(scripts_error);
        return (scripts_error);
    }
    this->_lines.clear();
    game_dialogue_table_clear_line_cache(this->_line_cache);
    this->_scripts.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_table::initialize(const game_dialogue_table &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_table::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = game_dialogue_table::_last_error;
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_lines.copy_from(other._lines);
        if (result == FT_ERR_SUCCESS)
            result = this->_scripts.copy_from(other._scripts);
        if (result == FT_ERR_SUCCESS)
        {
            result = other._line_cache.visit_entries(
                [this](const Pair<int32_t, game_dialogue_line *> &entry) noexcept -> int32_t
                {
                    game_dialogue_line *clone = ft_nullptr;
                    if (entry.value != ft_nullptr)
                    {
                        const int32_t clone_result = game_dialogue_table::clone_line(*entry.value, clone);
                        if (clone_result != FT_ERR_SUCCESS)
                            return (clone_result);
                    }
                    this->_line_cache.insert(entry.key, clone);
                    const int32_t insert_error = this->_line_cache.get_error();
                    if (insert_error != FT_ERR_SUCCESS)
                        delete clone;
                    return (insert_error);
                });
        }
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_table::initialize(game_dialogue_table &&other) noexcept
{
    return (this->move(other));
}

int32_t game_dialogue_table::move(game_dialogue_table &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_table::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = game_dialogue_table::_last_error;
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    ft_bool lines_locked = FT_FALSE;
    ft_bool scripts_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._lines.lock(&lines_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._scripts.lock(&scripts_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_lines.move(other._lines);
        const ft_bool lines_moved = result == FT_ERR_SUCCESS;
        if (lines_moved)
            result = this->_scripts.move(other._scripts);
        const ft_bool scripts_moved = result == FT_ERR_SUCCESS;
        if (scripts_moved)
            result = this->_line_cache.move(other._line_cache);
        if (result != FT_ERR_SUCCESS)
        {
            if (scripts_moved)
                (void)other._scripts.move(this->_scripts);
            if (lines_moved)
                (void)other._lines.move(this->_lines);
        }
    }
    if (scripts_locked)
        (void)other._scripts.unlock(scripts_locked);
    if (lines_locked)
        (void)other._lines.unlock(lines_locked);
    (void)other.unlock_internal(source_locked);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_line_cache.destroy();
        (void)this->_scripts.destroy();
        (void)this->_lines.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_table::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    const int32_t lines_error = this->_lines.destroy();
    if (result == FT_ERR_SUCCESS)
        result = lines_error;
    game_dialogue_table_clear_line_cache(this->_line_cache);
    const int32_t cache_error = this->_line_cache.destroy();
    if (result == FT_ERR_SUCCESS)
        result = cache_error;
    const int32_t scripts_error = this->_scripts.destroy();
    if (result == FT_ERR_SUCCESS)
        result = scripts_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
}

int32_t game_dialogue_table::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::enable_thread_safety");
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

int32_t game_dialogue_table::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

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

ft_bool game_dialogue_table::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_dialogue_table::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

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

int32_t game_dialogue_table::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_table::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::lock");
    return (this->lock_internal(lock_acquired));
}

void game_dialogue_table::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_dialogue_table::insert_line_snapshot(int32_t line_id,
    ft_sharedptr<game_dialogue_line> &stored, game_dialogue_line *cached) noexcept
{
    game_dialogue_line **cache_slot = ft_nullptr;
    int32_t result = this->_line_cache.visit_value(line_id,
        [&cache_slot](game_dialogue_line *&slot) noexcept
        {
            cache_slot = &slot;
            return (FT_ERR_SUCCESS);
        });
    if (result == FT_ERR_NOT_FOUND)
    {
        this->_line_cache.insert(line_id, cached);
        result = this->_line_cache.get_error();
    }
    if (result != FT_ERR_SUCCESS)
        return (result);

    this->_lines.insert(line_id, ft_move(stored));
    result = this->_lines.get_error();
    if (result == FT_ERR_SUCCESS)
    {
        if (cache_slot != ft_nullptr)
        {
            delete *cache_slot;
            *cache_slot = cached;
        }
    }
    else if (cache_slot == ft_nullptr)
        this->_line_cache.remove(line_id);
    return (result);
}

int32_t game_dialogue_table::register_line(const game_dialogue_line &line) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_table::register_line");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    ft_sharedptr<game_dialogue_line> stored;
    game_dialogue_line *view = ft_nullptr;
    result = game_dialogue_table::clone_line(line, view);
    if (result == FT_ERR_SUCCESS)
    {
        result = stored.initialize(view, FT_FALSE, 0U);
        if (result != FT_ERR_SUCCESS)
            delete view;
    }
    if (result == FT_ERR_SUCCESS)
    {
        game_dialogue_line *cached = ft_nullptr;
        result = game_dialogue_table::clone_line(*view, cached);
        if (result == FT_ERR_SUCCESS)
            result = this->insert_line_snapshot(view->_line_id, stored, cached);
        if (result != FT_ERR_SUCCESS)
            delete cached;
    }
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_dialogue_table::register_script(const game_dialogue_script &script) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::register_script");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        // Clone before replacing a live map entry.
        game_dialogue_script staged;
        result = staged.initialize(script);
        if (result == FT_ERR_SUCCESS)
        {
            const int32_t script_id = staged.get_script_id();
            this->_scripts.insert(script_id, ft_move(staged));
            result = this->_scripts.get_error();
        }
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_dialogue_table::fetch_line(int32_t line_id,
    game_dialogue_line &out_line) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_table::fetch_line");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_line_cache.visit_value(line_id,
            [&out_line](game_dialogue_line * const &line) noexcept -> int32_t
            {
                if (line == ft_nullptr || !line->is_initialised())
                    return (FT_ERR_NOT_FOUND);
                return (out_line.copy_values(*line));
            });
        if (result == FT_ERR_NOT_FOUND)
        {
            result = this->_lines.visit_value(line_id,
                [&out_line](const ft_sharedptr<game_dialogue_line> &stored) noexcept -> int32_t
                {
                    const game_dialogue_line *line = stored.get();
                    if (line == ft_nullptr || !line->is_initialised())
                        return (FT_ERR_NOT_FOUND);
                    return (out_line.copy_values(*line));
                });
        }
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_dialogue_table::fetch_script(int32_t script_id,
    game_dialogue_script &out_script) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_table::fetch_script");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_scripts.visit_value(script_id,
            [&out_script](const game_dialogue_script &stored) noexcept
            {
                return (out_script.initialize(stored));
            });
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &game_dialogue_table::get_lines() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::get_lines");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_lines);
}

const ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &game_dialogue_table::get_lines() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::get_lines const");
    const_cast<game_dialogue_table *>(this)->set_error(FT_ERR_SUCCESS);
    return (this->_lines);
}

void game_dialogue_table::set_lines(
    const ft_map<int32_t, ft_sharedptr<game_dialogue_line> > &lines) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_table::set_lines");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return ;
    }

    ft_map<int32_t, ft_sharedptr<game_dialogue_line>> staged_lines(0U);
    ft_map<int32_t, game_dialogue_line *> staged_cache(0U);
    result = staged_lines.initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged_cache.initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged_lines.copy_from(lines);
    if (result == FT_ERR_SUCCESS)
    {
        result = staged_lines.visit_entries(
            [&staged_cache](const Pair<int32_t, ft_sharedptr<game_dialogue_line>> &entry) noexcept -> int32_t
            {
                const game_dialogue_line *line = entry.value.get();
                if (line == ft_nullptr || !line->is_initialised())
                    return (FT_ERR_SUCCESS);
                game_dialogue_line *clone = ft_nullptr;
                const int32_t clone_result = game_dialogue_table::clone_line(*line, clone);
                if (clone_result != FT_ERR_SUCCESS)
                    return (clone_result);
                staged_cache.insert(entry.key, clone);
                const int32_t insert_error = staged_cache.get_error();
                if (insert_error != FT_ERR_SUCCESS)
                    delete clone;
                return (insert_error);
            });
    }
    if (result == FT_ERR_SUCCESS)
        result = this->_lines.move(staged_lines);
    if (result == FT_ERR_SUCCESS)
    {
        game_dialogue_table_clear_line_cache(this->_line_cache);
        // Both cache maps are private and have no independent mutex.
        (void)this->_line_cache.move(staged_cache);
    }
    if (staged_cache.is_initialised() == FT_CLASS_STATE_INITIALISED)
        game_dialogue_table_clear_line_cache(staged_cache);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

ft_map<int32_t, game_dialogue_script> &game_dialogue_table::get_scripts() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::get_scripts");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_scripts);
}

const ft_map<int32_t, game_dialogue_script> &game_dialogue_table::get_scripts() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_table::get_scripts const");
    const_cast<game_dialogue_table *>(this)->set_error(FT_ERR_SUCCESS);
    return (this->_scripts);
}

void game_dialogue_table::set_scripts(
    const ft_map<int32_t, game_dialogue_script> &scripts) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_table::set_scripts");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_scripts.copy_from(scripts);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}
