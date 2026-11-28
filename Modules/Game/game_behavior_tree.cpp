#include "game_behavior_tree.hpp"
#include "../Template/move.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>

thread_local int32_t game_behavior_node::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_behavior_tree::_last_error = FT_ERR_SUCCESS;

game_behavior_context::game_behavior_context() noexcept
    : _character(ft_nullptr), _user_data(ft_nullptr)
{
    return ;
}

game_behavior_context::game_behavior_context(const game_behavior_context &other) noexcept
    : _character(other._character), _user_data(other._user_data)
{
    return ;
}

game_behavior_context::game_behavior_context(game_behavior_context &&other) noexcept
    : _character(other._character), _user_data(other._user_data)
{
    other._character = ft_nullptr;
    other._user_data = ft_nullptr;
    return ;
}

game_behavior_context::~game_behavior_context() noexcept
{
    return ;
}

game_character *game_behavior_context::get_character() const noexcept
{
    return (this->_character);
}

void game_behavior_context::set_character(game_character *character) noexcept
{
    this->_character = character;
    return ;
}

void *game_behavior_context::get_user_data() const noexcept
{
    return (this->_user_data);
}

void game_behavior_context::set_user_data(void *user_data) noexcept
{
    this->_user_data = user_data;
    return ;
}

game_behavior_node::game_behavior_node() noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_behavior_node::game_behavior_node(const game_behavior_node &other) noexcept
{
    this->set_error(other.get_error());
    return ;
}

game_behavior_node::game_behavior_node(game_behavior_node &&other) noexcept
{
    this->set_error(other.get_error());
    return ;
}

game_behavior_node::~game_behavior_node() noexcept
{
    return ;
}

int32_t game_behavior_node::set_error(int32_t error_code) noexcept
{
    game_behavior_node::_last_error = error_code;
    return (error_code);
}

int32_t game_behavior_node::get_error() const noexcept
{
    return (game_behavior_node::_last_error);
}

const char *game_behavior_node::get_error_str() const noexcept
{
    return (ft_strerror(game_behavior_node::_last_error));
}

game_behavior_tree_action::game_behavior_tree_action() noexcept
    : game_behavior_node(), _callback(), _active_callback(ft_nullptr)
{
    return ;
}

game_behavior_tree_action::game_behavior_tree_action(
    const game_behavior_tree_action &other) noexcept
    : game_behavior_node(other), _callback(), _active_callback(ft_nullptr)
{
    const int32_t source_error = other.get_error();
    this->set_callback(other._callback);
    if (this->get_error() == FT_ERR_SUCCESS)
        this->set_error(source_error);
    return ;
}

game_behavior_tree_action::game_behavior_tree_action(
    game_behavior_tree_action &&other) noexcept
    : game_behavior_node(static_cast<game_behavior_node &&>(other)),
      _callback(), _active_callback(ft_nullptr)
{
    if (other._active_callback != ft_nullptr)
    {
        const int32_t source_error = other.get_error();
        this->set_callback(other._callback);
        if (this->get_error() == FT_ERR_SUCCESS)
            this->set_error(source_error);
    }
    else
        this->_callback = ft_move(other._callback);
    return ;
}

game_behavior_tree_action::~game_behavior_tree_action() noexcept
{
    return ;
}

void game_behavior_tree_action::set_callback(
    const ft_function<int32_t(game_behavior_context &)> &callback) noexcept
{
    if (&callback == &this->_callback)
    {
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    try
    {
        ft_function<int32_t(game_behavior_context &)> replacement(callback);
        if (callback && !replacement)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            return ;
        }
        if (this->_active_callback != ft_nullptr)
        {
            *this->_active_callback = ft_move(this->_callback);
            this->_active_callback = ft_nullptr;
        }
        this->_callback = ft_move(replacement);
        this->set_error(FT_ERR_SUCCESS);
    }
    catch (const std::bad_alloc &)
    {
        this->set_error(FT_ERR_NO_MEMORY);
    }
    catch (...)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
    }
    return ;
}

const ft_function<int32_t(game_behavior_context &)>
    &game_behavior_tree_action::get_callback() const noexcept
{
    return (this->_callback);
}

int32_t game_behavior_tree_action::tick(game_behavior_context &context) noexcept
{
    if (!this->_callback)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    int32_t status = -1;
    {
        ft_function<int32_t(game_behavior_context &)> retired_callback;
        if (this->_active_callback == ft_nullptr)
            this->_active_callback = &retired_callback;
        try
        {
            status = this->_callback(context);
        }
        catch (...)
        {
            status = -1;
        }
        if (this->_active_callback == &retired_callback)
            this->_active_callback = ft_nullptr;
    }
    if (status != FT_BEHAVIOR_STATUS_RUNNING
        && status != FT_BEHAVIOR_STATUS_SUCCESS
        && status != FT_BEHAVIOR_STATUS_FAILURE)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (status);
}

game_behavior_composite::game_behavior_composite() noexcept
    : game_behavior_node(), _children(),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t game_behavior_composite::initialize() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = this->_children.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_children.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_behavior_composite::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    error_code = this->_children.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(error_code));
}

int32_t game_behavior_composite::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_behavior_composite::get_error");
    return (game_behavior_node::get_error());
}

const char *game_behavior_composite::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_behavior_composite::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_behavior_composite::move(game_behavior_composite &other) noexcept
{
    int32_t error_code;

    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_behavior_composite::move", "source is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    error_code = this->_children.move(other._children);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

game_behavior_composite::~game_behavior_composite() noexcept
{
    (void)this->destroy();
    return ;
}

ft_bool game_behavior_composite::validate_child(const ft_sharedptr<game_behavior_node> &child) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        const_cast<game_behavior_composite *>(this)->set_error(
            FT_ERR_NOT_INITIALISED);
        return (FT_FALSE);
    }
    const game_behavior_node *node = child.get();
    if (node == ft_nullptr)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    if (node->get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (FT_TRUE);
}

void game_behavior_composite::add_child(const ft_sharedptr<game_behavior_node> &child) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return ;
    }
    if (!child)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    this->set_error(this->_children.push_back(child));
    return ;
}

void game_behavior_composite::clear_children() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return ;
    }
    this->_children.clear();
    this->set_error(this->_children.get_error());
    return ;
}

ft_vector<ft_sharedptr<game_behavior_node> > &game_behavior_composite::get_children() noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_children);
}

const ft_vector<ft_sharedptr<game_behavior_node> > &game_behavior_composite::get_children() const noexcept
{
    const_cast<game_behavior_composite *>(this)->set_error(FT_ERR_SUCCESS);
    return (this->_children);
}

game_behavior_selector::game_behavior_selector() noexcept
    : game_behavior_composite()
{
    return ;
}

game_behavior_selector::~game_behavior_selector() noexcept
{
    return ;
}

int32_t game_behavior_composite::tick_children(game_behavior_context &context,
    int32_t stop_status, int32_t completed_status) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }

    const ft_size_t child_count = this->_children.size();
    const int32_t size_error = this->_children.get_error();
    if (size_error != FT_ERR_SUCCESS)
    {
        this->set_error(size_error);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    ft_size_t index = 0;
    while (index < child_count)
    {
        ft_sharedptr<game_behavior_node> &handle = this->_children[index];
        const int32_t lookup_error = this->_children.get_error();
        if (lookup_error != FT_ERR_SUCCESS)
        {
            this->set_error(lookup_error);
            return (FT_BEHAVIOR_STATUS_FAILURE);
        }
        int32_t status = FT_BEHAVIOR_STATUS_FAILURE;
        int32_t child_error = FT_ERR_SUCCESS;
        const int32_t visit_error = handle.visit_retained(
            [&context, &status, &child_error](game_behavior_node &child) noexcept
            {
                child_error = child.get_error();
                if (child_error == FT_ERR_SUCCESS)
                {
                    status = child.tick(context);
                    child_error = child.get_error();
                }
            });
        if (visit_error != FT_ERR_SUCCESS)
        {
            if (visit_error == FT_ERR_INVALID_POINTER || visit_error == FT_ERR_INVALID_STATE)
                this->set_error(FT_ERR_INVALID_ARGUMENT);
            else
                this->set_error(visit_error);
            return (FT_BEHAVIOR_STATUS_FAILURE);
        }
        if (this->game_behavior_node::get_error() != child_error)
            this->set_error(child_error);
        if (child_error != FT_ERR_SUCCESS)
            return (FT_BEHAVIOR_STATUS_FAILURE);
        if (status == stop_status || status == FT_BEHAVIOR_STATUS_RUNNING)
            return (status);
        ++index;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (completed_status);
}

int32_t game_behavior_selector::tick(game_behavior_context &context) noexcept
{
    return (this->tick_children(context, FT_BEHAVIOR_STATUS_SUCCESS,
        FT_BEHAVIOR_STATUS_FAILURE));
}

game_behavior_sequence::game_behavior_sequence() noexcept
    : game_behavior_composite()
{
    return ;
}

game_behavior_sequence::~game_behavior_sequence() noexcept
{
    return ;
}

int32_t game_behavior_sequence::tick(game_behavior_context &context) noexcept
{
    return (this->tick_children(context, FT_BEHAVIOR_STATUS_FAILURE,
        FT_BEHAVIOR_STATUS_SUCCESS));
}

game_behavior_tree::game_behavior_tree() noexcept
    : _root(), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t game_behavior_tree::initialize() noexcept
{
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_INVALID_STATE));
    error_code = this->_root.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)this->_root.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_behavior_tree::destroy() noexcept
{
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    error_code = this->_root.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(error_code));
}

int32_t game_behavior_tree::move(game_behavior_tree &other) noexcept
{
    int32_t error_code;

    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_behavior_tree::move", "source is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        error_code = this->destroy();
        if (error_code != FT_ERR_SUCCESS)
            return (this->set_error(error_code));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_SUCCESS));
    }
    error_code = this->_root.move(other._root);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

game_behavior_tree::~game_behavior_tree() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_behavior_tree::set_error(int32_t error_code) noexcept
{
    game_behavior_tree::_last_error = error_code;
    return (error_code);
}

void game_behavior_tree::set_root(const ft_sharedptr<game_behavior_node> &root) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return ;
    }
    this->_root = root;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_sharedptr<game_behavior_node> &game_behavior_tree::get_root() noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_root);
}

const ft_sharedptr<game_behavior_node> &game_behavior_tree::get_root() const noexcept
{
    const_cast<game_behavior_tree *>(this)->set_error(FT_ERR_SUCCESS);
    return (this->_root);
}

int32_t game_behavior_tree::tick(game_behavior_context &context) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    int32_t status = FT_BEHAVIOR_STATUS_FAILURE;
    int32_t root_error = FT_ERR_SUCCESS;
    const int32_t visit_error = this->_root.visit_retained(
        [&context, &status, &root_error](game_behavior_node &root) noexcept
        {
            status = root.tick(context);
            root_error = root.get_error();
        });
    if (visit_error != FT_ERR_SUCCESS)
    {
        if (visit_error == FT_ERR_INVALID_POINTER || visit_error == FT_ERR_INVALID_STATE)
            this->set_error(FT_ERR_INVALID_ARGUMENT);
        else
            this->set_error(visit_error);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    this->set_error(root_error);
    return (status);
}

int32_t game_behavior_tree::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_behavior_tree::get_error");
    return (game_behavior_tree::_last_error);
}

const char *game_behavior_tree::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_behavior_tree::get_error_str");
    return (ft_strerror(game_behavior_tree::_last_error));
}
