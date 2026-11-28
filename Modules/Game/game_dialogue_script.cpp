#include "../PThread/pthread_internal.hpp"
#include "game_dialogue_script.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include "game_dialogue_line.hpp"

thread_local int32_t game_dialogue_script::_last_error = FT_ERR_SUCCESS;

static const game_dialogue_line *game_dialogue_line_pointer(const game_dialogue_line &line)
{
    return (&line);
}

static const game_dialogue_line *game_dialogue_line_pointer(
    const ft_sharedptr<game_dialogue_line> &line)
{
    return (line.get());
}

int32_t game_dialogue_script::clone_line(const game_dialogue_line *line,
    ft_sharedptr<game_dialogue_line> &owned_line) noexcept
{
    if (line == ft_nullptr)
        return (owned_line.initialize());
    if (!line->is_initialised())
        return (FT_ERR_INVALID_STATE);
    game_dialogue_line *clone = new (std::nothrow) game_dialogue_line();
    if (clone == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t result = clone->initialize();
    if (result == FT_ERR_SUCCESS)
        result = clone->copy_values(*line);
    if (result == FT_ERR_SUCCESS)
        result = owned_line.initialize(clone, FT_FALSE, 0U);
    if (result != FT_ERR_SUCCESS)
        delete clone;
    return (result);
}

template <typename LineType>
int32_t game_dialogue_script::stage_lines(const ft_vector<LineType> &source,
    ft_vector<ft_sharedptr<game_dialogue_line>> &staged) noexcept
{
    int32_t result = staged.initialize();
    if (result != FT_ERR_SUCCESS || source.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (result);
    ft_bool source_locked = FT_FALSE;
    result = source.lock(&source_locked);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const ft_size_t count = source.size();
    result = source.get_error();
    if (result == FT_ERR_SUCCESS)
    {
        staged.reserve(count);
        result = staged.get_error();
    }
    const LineType *entry = ft_nullptr;
    if (result == FT_ERR_SUCCESS && count != 0U)
    {
        entry = source.begin();
        result = source.get_error();
    }
    ft_size_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < count)
    {
        ft_sharedptr<game_dialogue_line> clone;
        result = game_dialogue_script::clone_line(game_dialogue_line_pointer(entry[index]), clone);
        if (result == FT_ERR_SUCCESS)
            result = staged.push_back(ft_move(clone));
        ++index;
    }
    source.unlock(source_locked);
    return (result);
}

int32_t game_dialogue_script::set_error(int32_t error_code) noexcept
{
    game_dialogue_script::_last_error = error_code;
    return (error_code);
}

game_dialogue_script::game_dialogue_script() noexcept
    : _script_id(0), _title(), _summary(), _start_line_id(0), _lines(),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

game_dialogue_script::~game_dialogue_script() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_dialogue_script::initialize() noexcept
{
    int32_t lines_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_dialogue_script::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_script_id = 0;
    int32_t title_error = this->_title.initialize();
    if (title_error != FT_ERR_SUCCESS)
        return (title_error);
    int32_t summary_error = this->_summary.initialize();
    if (summary_error != FT_ERR_SUCCESS)
    {
        (void)this->_title.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (summary_error);
    }
    lines_error = this->_lines.initialize();
    if (lines_error != FT_ERR_SUCCESS)
    {
        (void)this->_title.destroy();
        (void)this->_summary.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lines_error);
    }
    this->_start_line_id = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::initialize(const game_dialogue_script &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_script::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = game_dialogue_script::_last_error;
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
            result = this->_title.assign(other._title);
        if (result == FT_ERR_SUCCESS)
            result = this->_summary.assign(other._summary);
        ft_vector<ft_sharedptr<game_dialogue_line>> staged;
        if (result == FT_ERR_SUCCESS)
            result = game_dialogue_script::stage_lines(other._lines, staged);
        if (result == FT_ERR_SUCCESS)
            result = this->_lines.move(staged);
        if (result == FT_ERR_SUCCESS)
        {
            this->_script_id = other._script_id;
            this->_start_line_id = other._start_line_id;
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

int32_t game_dialogue_script::initialize(game_dialogue_script &&other) noexcept
{
    return (this->move(other));
}

int32_t game_dialogue_script::move(game_dialogue_script &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_script::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = game_dialogue_script::_last_error;
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        if (other._lines.is_initialised() == FT_CLASS_STATE_INITIALISED)
            result = this->_lines.move(other._lines);
        if (result == FT_ERR_SUCCESS)
        {
            // Both private strings use the script mutex and transfer without allocation.
            (void)this->_title.move(other._title);
            (void)this->_summary.move(other._summary);
            this->_script_id = other._script_id;
            this->_start_line_id = other._start_line_id;
        }
        (void)other.unlock_internal(source_locked);
    }
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::initialize(int32_t script_id, const ft_string &title,
    const ft_string &summary, int32_t start_line_id,
    const ft_vector<game_dialogue_line> &lines) noexcept
{
    ft_string staged_title;
    ft_string staged_summary;
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    // Metadata arguments can borrow the current script's strings.
    if (title.is_initialised() && summary.is_initialised())
        result = staged_title.initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged_summary.initialize();
    if (result == FT_ERR_SUCCESS)
        result = staged_title.assign(title);
    if (result == FT_ERR_SUCCESS)
        result = staged_summary.assign(summary);
    ft_vector<ft_sharedptr<game_dialogue_line>> staged;
    if (result == FT_ERR_SUCCESS)
        result = game_dialogue_script::stage_lines(lines, staged);
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (result == FT_ERR_SUCCESS)
        result = this->initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_lines.move(staged);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    (void)this->_title.move(staged_title);
    (void)this->_summary.move(staged_summary);
    this->_script_id = script_id;
    this->_start_line_id = start_line_id;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_dialogue_script::destroy() noexcept
{
    int32_t lines_error;
    int32_t title_error;
    int32_t summary_error;
    int32_t first_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    first_error = disable_error;
    lines_error = this->_lines.destroy();
    if (first_error == FT_ERR_SUCCESS && lines_error != FT_ERR_SUCCESS)
        first_error = lines_error;
    title_error = this->_title.destroy();
    if (first_error == FT_ERR_SUCCESS && title_error != FT_ERR_SUCCESS)
        first_error = title_error;
    summary_error = this->_summary.destroy();
    if (first_error == FT_ERR_SUCCESS && summary_error != FT_ERR_SUCCESS)
        first_error = summary_error;
    this->_script_id = 0;
    this->_start_line_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(first_error));
}

int32_t game_dialogue_script::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool game_dialogue_script::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_dialogue_script::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_dialogue_script::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::lock");
    return (this->lock_internal(lock_acquired));
}

void game_dialogue_script::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_dialogue_script::get_script_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_script_id");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    const int32_t script_id = this->_script_id;
    (void)this->unlock_internal(lock_acquired);
    return (script_id);
}

void game_dialogue_script::set_script_id(int32_t script_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_script_id");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_script_id = script_id;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

const ft_string &game_dialogue_script::get_title() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_title");
    return (this->_title);
}

void game_dialogue_script::set_title(const ft_string &title) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_title");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = FT_ERR_INVALID_ARGUMENT;
        if (title.is_initialised())
            result = this->_title.assign(title);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

const ft_string &game_dialogue_script::get_summary() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_summary");
    return (this->_summary);
}

void game_dialogue_script::set_summary(const ft_string &summary) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_summary");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = FT_ERR_INVALID_ARGUMENT;
        if (summary.is_initialised())
            result = this->_summary.assign(summary);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_dialogue_script::get_start_line_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_start_line_id");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    const int32_t start_line_id = this->_start_line_id;
    (void)this->unlock_internal(lock_acquired);
    return (start_line_id);
}

void game_dialogue_script::set_start_line_id(int32_t start_line_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_start_line_id");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_start_line_id = start_line_id;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

const ft_vector<ft_sharedptr<game_dialogue_line> > &game_dialogue_script::get_lines() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_lines const");
    return (this->_lines);
}

ft_vector<ft_sharedptr<game_dialogue_line> > &game_dialogue_script::get_lines() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_lines");
    return (this->_lines);
}

void game_dialogue_script::set_lines(
    const ft_vector<ft_sharedptr<game_dialogue_line> > &lines) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_lines");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        if (&lines != &this->_lines)
        {
            ft_vector<ft_sharedptr<game_dialogue_line>> staged;
            result = game_dialogue_script::stage_lines(lines, staged);
            if (result == FT_ERR_SUCCESS)
                result = this->_lines.replace_from(staged);
        }
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


int32_t game_dialogue_script::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_script::get_error");
    return (game_dialogue_script::_last_error);
}

const char *game_dialogue_script::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_script::get_error_str");
    return (ft_strerror(game_dialogue_script::_last_error));
}
