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

static void game_dialogue_copy_next_ids(const ft_vector<int32_t> &source,
    ft_vector<int32_t> &destination)
{
    const int32_t *entry;
    const int32_t *entry_end;

    destination.clear();
    entry = source.begin();
    entry_end = source.end();
    while (entry != entry_end)
    {
        destination.push_back(*entry);
        ++entry;
    }
    return ;
}

static ft_sharedptr<game_dialogue_line> game_dialogue_clone_line_ptr(
    const ft_sharedptr<game_dialogue_line> &line)
{
    game_dialogue_line *cloned_line;
    ft_vector<int32_t> copied_next_ids;
    int32_t initialize_error;

    if (line == ft_sharedptr<game_dialogue_line>())
        return (ft_sharedptr<game_dialogue_line>());
    if (copied_next_ids.initialize() != FT_ERR_SUCCESS)
        return (ft_sharedptr<game_dialogue_line>());
    cloned_line = new (std::nothrow) game_dialogue_line();
    if (cloned_line == ft_nullptr)
        return (ft_sharedptr<game_dialogue_line>());
    initialize_error = cloned_line->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete cloned_line;
        return (ft_sharedptr<game_dialogue_line>());
    }
    cloned_line->set_line_id(line->get_line_id());
    cloned_line->set_speaker(line->get_speaker());
    cloned_line->set_text(line->get_text());
    game_dialogue_copy_next_ids(line->get_next_line_ids(), copied_next_ids);
    cloned_line->set_next_line_ids(copied_next_ids);
    return (ft_sharedptr<game_dialogue_line>(cloned_line));
}

static void game_dialogue_copy_line_vector(
    const ft_vector<ft_sharedptr<game_dialogue_line> > &source,
    ft_vector<ft_sharedptr<game_dialogue_line> > &destination)
{
    const ft_sharedptr<game_dialogue_line> *entry;
    const ft_sharedptr<game_dialogue_line> *entry_end;

    destination.clear();
    entry = source.begin();
    entry_end = source.end();
    while (entry != entry_end)
    {
        destination.push_back(game_dialogue_clone_line_ptr(*entry));
        ++entry;
    }
    return ;
}

static void game_dialogue_copy_plain_line_vector(const ft_vector<game_dialogue_line> &source,
    ft_vector<ft_sharedptr<game_dialogue_line> > &destination)
{
    const game_dialogue_line *entry;
    const game_dialogue_line *entry_end;
    game_dialogue_line *cloned_line;
    ft_vector<int32_t> copied_next_ids;
    int32_t initialize_error;

    if (copied_next_ids.initialize() != FT_ERR_SUCCESS)
        return ;
    destination.clear();
    entry = source.begin();
    entry_end = source.end();
    while (entry != entry_end)
    {
        cloned_line = new (std::nothrow) game_dialogue_line();
        if (cloned_line == ft_nullptr)
        {
            destination.push_back(ft_sharedptr<game_dialogue_line>());
            ++entry;
            continue ;
        }
        initialize_error = cloned_line->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            delete cloned_line;
            destination.push_back(ft_sharedptr<game_dialogue_line>());
            ++entry;
            continue ;
        }
        cloned_line->set_line_id(entry->get_line_id());
        cloned_line->set_speaker(entry->get_speaker());
        cloned_line->set_text(entry->get_text());
        game_dialogue_copy_next_ids(entry->get_next_line_ids(), copied_next_ids);
        cloned_line->set_next_line_ids(copied_next_ids);
        destination.push_back(ft_sharedptr<game_dialogue_line>(cloned_line));
        ++entry;
    }
    return ;
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
    int32_t initialize_error;
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_script::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_script_id = other._script_id;
    this->_title = other._title;
    this->_summary = other._summary;
    this->_start_line_id = other._start_line_id;
    game_dialogue_copy_line_vector(other._lines, this->_lines);
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::initialize(game_dialogue_script &&other) noexcept
{
    return (this->move(other));
}

int32_t game_dialogue_script::move(game_dialogue_script &other) noexcept
{
    int32_t initialize_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_script::move",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_dialogue_script &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t game_dialogue_script::initialize(int32_t script_id, const ft_string &title,
    const ft_string &summary, int32_t start_line_id,
    const ft_vector<game_dialogue_line> &lines) noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_script_id = script_id;
    this->_title = title;
    this->_summary = summary;
    this->_start_line_id = start_line_id;
    game_dialogue_copy_plain_line_vector(lines, this->_lines);
    return (FT_ERR_SUCCESS);
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
    first_error = FT_ERR_SUCCESS;
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
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (first_error != FT_ERR_SUCCESS)
        return (first_error);
    return (disable_error);
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
    return (this->_script_id);
}

void game_dialogue_script::set_script_id(int32_t script_id) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_script_id");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_script_id = script_id;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const ft_string &game_dialogue_script::get_title() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_title");
    return (this->_title);
}

void game_dialogue_script::set_title(const ft_string &title) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_title");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_title = title;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const ft_string &game_dialogue_script::get_summary() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_summary");
    return (this->_summary);
}

void game_dialogue_script::set_summary(const ft_string &summary) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_summary");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_summary = summary;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_dialogue_script::get_start_line_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::get_start_line_id");
    return (this->_start_line_id);
}

void game_dialogue_script::set_start_line_id(int32_t start_line_id) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_start_line_id");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->_start_line_id = start_line_id;
    (void)this->unlock_internal(lock_acquired);
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
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_script::set_lines");
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    game_dialogue_copy_line_vector(lines, this->_lines);
    (void)this->unlock_internal(lock_acquired);
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
    return (ft_strerror(this->get_error()));
}
