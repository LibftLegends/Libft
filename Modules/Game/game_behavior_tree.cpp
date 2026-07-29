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
    : game_behavior_node(), _callback()
{
    return ;
}

game_behavior_tree_action::game_behavior_tree_action(
    const game_behavior_tree_action &other) noexcept
    : game_behavior_node(other), _callback(other._callback)
{
    return ;
}

game_behavior_tree_action::game_behavior_tree_action(
    game_behavior_tree_action &&other) noexcept
    : game_behavior_node(static_cast<game_behavior_node &&>(other)),
      _callback(ft_move(other._callback))
{
    return ;
}

game_behavior_tree_action::~game_behavior_tree_action() noexcept
{
    return ;
}

void game_behavior_tree_action::set_callback(
    const ft_function<int32_t(game_behavior_context &)> &callback) noexcept
{
    this->_callback = callback;
    this->set_error(FT_ERR_SUCCESS);
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
    int32_t status;

    status = this->_callback(context);
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
    if (!child)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    if (child->get_error() != FT_ERR_SUCCESS)
    {
        this->set_error(child->get_error());
        return (FT_FALSE);
    }
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
    this->_children.push_back(child);
    this->set_error(FT_ERR_SUCCESS);
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
    this->set_error(FT_ERR_SUCCESS);
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

int32_t game_behavior_selector::tick(game_behavior_context &context) noexcept
{
    ft_size_t index;
    ft_size_t child_count;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    index = 0;
    child_count = this->_children.size();
    while (index < child_count)
    {
        ft_sharedptr<game_behavior_node> &child = this->_children[index];
        if (this->validate_child(child) == FT_FALSE)
            return (FT_BEHAVIOR_STATUS_FAILURE);
        int32_t status = child->tick(context);
        if (child->get_error() != FT_ERR_SUCCESS)
        {
            this->set_error(child->get_error());
            return (FT_BEHAVIOR_STATUS_FAILURE);
        }
        if (status == FT_BEHAVIOR_STATUS_SUCCESS)
        {
            this->set_error(FT_ERR_SUCCESS);
            return (FT_BEHAVIOR_STATUS_SUCCESS);
        }
        if (status == FT_BEHAVIOR_STATUS_RUNNING)
        {
            this->set_error(FT_ERR_SUCCESS);
            return (FT_BEHAVIOR_STATUS_RUNNING);
        }
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_BEHAVIOR_STATUS_FAILURE);
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
    ft_size_t index;
    ft_size_t child_count;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    index = 0;
    child_count = this->_children.size();
    if (child_count == 0)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_BEHAVIOR_STATUS_SUCCESS);
    }
    while (index < child_count)
    {
        ft_sharedptr<game_behavior_node> &child = this->_children[index];
        if (this->validate_child(child) == FT_FALSE)
            return (FT_BEHAVIOR_STATUS_FAILURE);
        int32_t status = child->tick(context);
        if (child->get_error() != FT_ERR_SUCCESS)
        {
            this->set_error(child->get_error());
            return (FT_BEHAVIOR_STATUS_FAILURE);
        }
        if (status == FT_BEHAVIOR_STATUS_FAILURE)
        {
            this->set_error(FT_ERR_SUCCESS);
            return (FT_BEHAVIOR_STATUS_FAILURE);
        }
        if (status == FT_BEHAVIOR_STATUS_RUNNING)
        {
            this->set_error(FT_ERR_SUCCESS);
            return (FT_BEHAVIOR_STATUS_RUNNING);
        }
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_BEHAVIOR_STATUS_SUCCESS);
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
    if (!this->_root)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_BEHAVIOR_STATUS_FAILURE);
    }
    int32_t status = this->_root->tick(context);

    this->set_error(this->_root->get_error());
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
