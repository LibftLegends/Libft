#include "../PThread/pthread_internal.hpp"
#include "game_dialogue_line.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"

static int32_t game_dialogue_replace_int_vector(const ft_vector<int32_t> &source,
    ft_vector<int32_t> &destination)
{
    if (source.is_initialised() == FT_CLASS_STATE_INITIALISED)
        return (destination.replace_from(source));
    // Existing callers can supply a default-constructed vector for no links.
    destination.clear();
    return (destination.get_error());
}

thread_local int32_t game_dialogue_line::_last_error = FT_ERR_SUCCESS;

int32_t game_dialogue_line::set_error(int32_t error_code) noexcept
{
    game_dialogue_line::_last_error = error_code;
    return (error_code);
}

game_dialogue_line::game_dialogue_line() noexcept
    : _line_id(0), _speaker(), _text(), _next_line_ids(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_dialogue_line::~game_dialogue_line() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_dialogue_line::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_dialogue_line::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    int32_t result = this->_speaker.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_text.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_next_line_ids.initialize();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(result));
}

int32_t game_dialogue_line::initialize(int32_t line_id, const ft_string &speaker,
    const ft_string &text, const ft_vector<int32_t> &next_line_ids) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::initialize(values)");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->replace_values(line_id, speaker, text, next_line_ids);
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_dialogue_line::replace_values(int32_t line_id, const ft_string &speaker,
    const ft_string &text, const ft_vector<int32_t> &next_line_ids) noexcept
{
    ft_string staged_speaker;
    ft_string staged_text;
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (speaker.is_initialised() && text.is_initialised())
    {
        result = staged_speaker.initialize();
        if (result == FT_ERR_SUCCESS)
            result = staged_text.initialize();
        if (result == FT_ERR_SUCCESS)
            result = staged_speaker.assign(speaker);
        if (result == FT_ERR_SUCCESS)
            result = staged_text.assign(text);
        if (result == FT_ERR_SUCCESS)
            result = game_dialogue_replace_int_vector(next_line_ids, this->_next_line_ids);
        if (result == FT_ERR_SUCCESS)
        {
            // The private strings are synchronized by the line mutex. Their
            // buffers transfer without allocation or independent mutexes.
            (void)this->_speaker.move(staged_speaker);
            (void)this->_text.move(staged_text);
            this->_line_id = line_id;
        }
    }
    return (result);
}

int32_t game_dialogue_line::copy_values(const game_dialogue_line &source) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_dialogue_line::copy_values");
    if (!source.is_initialised())
        return (this->set_error(FT_ERR_INVALID_STATE));
    if (this == &source)
        return (this->set_error(FT_ERR_SUCCESS));

    const game_dialogue_line *first = &source;
    const game_dialogue_line *second = this;
    if (reinterpret_cast<uintptr_t>(first->_mutex) > reinterpret_cast<uintptr_t>(second->_mutex))
    {
        first = this;
        second = &source;
    }
    ft_bool first_locked = FT_FALSE;
    int32_t result = first->lock_internal(&first_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool second_locked = FT_FALSE;
    result = second->lock_internal(&second_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->replace_values(source._line_id, source._speaker,
            source._text, source._next_line_ids);
        second->unlock_internal(second_locked);
    }
    first->unlock_internal(first_locked);
    return (this->set_error(result));
}

int32_t game_dialogue_line::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    const int32_t speaker_error = this->_speaker.destroy();
    if (result == FT_ERR_SUCCESS)
        result = speaker_error;
    const int32_t text_error = this->_text.destroy();
    if (result == FT_ERR_SUCCESS)
        result = text_error;
    const int32_t links_error = this->_next_line_ids.destroy();
    if (result == FT_ERR_SUCCESS)
        result = links_error;
    this->_line_id = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(result));
}

int32_t game_dialogue_line::move(game_dialogue_line &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_dialogue_line::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = game_dialogue_line::_last_error;
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
        return (result);
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        if (other._next_line_ids.is_initialised() == FT_CLASS_STATE_INITIALISED)
            result = this->_next_line_ids.move(other._next_line_ids);
        if (result == FT_ERR_SUCCESS)
        {
            // The text fields are private and use the line mutex. After
            // the vector transfer succeeds, both string moves are infallible.
            (void)this->_speaker.move(other._speaker);
            (void)this->_text.move(other._text);
            this->_line_id = other._line_id;
        }
        other.unlock_internal(source_locked);
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

int32_t game_dialogue_line::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (this->_mutex != ft_nullptr);
    return (FT_ERR_SUCCESS);
}

void game_dialogue_line::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_dialogue_line::get_line_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::get_line_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    result = this->_line_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (result);
}

void game_dialogue_line::set_line_id(int32_t line_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::set_line_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_line_id = line_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const ft_string &game_dialogue_line::get_speaker() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const ft_string *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::get_speaker");
    result = &this->_speaker;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (*result);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (*result);
}

void game_dialogue_line::set_speaker(const ft_string &speaker) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::set_speaker");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (speaker.is_initialised())
        result = this->_speaker.assign(speaker);
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

const ft_string &game_dialogue_line::get_text() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const ft_string *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::get_text");
    result = &this->_text;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (*result);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (*result);
}

void game_dialogue_line::set_text(const ft_string &text) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::set_text");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (text.is_initialised())
        result = this->_text.assign(text);
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

const ft_vector<int32_t> &game_dialogue_line::get_next_line_ids() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const ft_vector<int32_t> *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::get_next_line_ids const");
    result = &this->_next_line_ids;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (*result);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (*result);
}

ft_vector<int32_t> &game_dialogue_line::get_next_line_ids() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_vector<int32_t> *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::get_next_line_ids");
    result = &this->_next_line_ids;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (*result);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (*result);
}

void game_dialogue_line::set_next_line_ids(const ft_vector<int32_t> &next_line_ids) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::set_next_line_ids");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const int32_t result = game_dialogue_replace_int_vector(next_line_ids, this->_next_line_ids);
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

int32_t game_dialogue_line::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::enable_thread_safety");
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

int32_t game_dialogue_line::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_dialogue_line::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_dialogue_line::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

int32_t game_dialogue_line::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_line::get_error");
    return (game_dialogue_line::_last_error);
}

const char *game_dialogue_line::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_dialogue_line::get_error_str");
    return (ft_strerror(game_dialogue_line::_last_error));
}

ft_bool game_dialogue_line::is_initialised() const noexcept
{
    return (this->_initialised_state == FT_CLASS_STATE_INITIALISED);
}
