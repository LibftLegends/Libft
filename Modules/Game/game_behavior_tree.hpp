#ifndef GAME_BEHAVIOR_TREE_HPP
# define GAME_BEHAVIOR_TREE_HPP

#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/function.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"

class game_character;

#define FT_BEHAVIOR_STATUS_RUNNING 0
#define FT_BEHAVIOR_STATUS_SUCCESS 1
#define FT_BEHAVIOR_STATUS_FAILURE 2

class game_behavior_context
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        game_character        *_character;
        void                *_user_data;

    public:
        game_behavior_context() noexcept;
        game_behavior_context(const game_behavior_context &other) noexcept;
        game_behavior_context(game_behavior_context &&other) noexcept;
        ~game_behavior_context() noexcept;
        game_behavior_context &operator=(const game_behavior_context &other) noexcept = delete;
        game_behavior_context &operator=(game_behavior_context &&other) noexcept = delete;

        game_character *get_character() const noexcept;
        void set_character(game_character *character) noexcept;

        void *get_user_data() const noexcept;
        void set_user_data(void *user_data) noexcept;
};

class game_behavior_node
{
    protected:
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_behavior_node() noexcept;
        game_behavior_node(const game_behavior_node &other) noexcept;
        game_behavior_node(game_behavior_node &&other) noexcept;
        virtual ~game_behavior_node() noexcept;
        game_behavior_node &operator=(const game_behavior_node &other) noexcept = delete;
        game_behavior_node &operator=(game_behavior_node &&other) noexcept = delete;

        virtual int32_t tick(game_behavior_context &context) noexcept = 0;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

class game_behavior_tree_action : public game_behavior_node
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_function<int32_t(game_behavior_context &)> _callback;

    public:
        game_behavior_tree_action() noexcept;
        game_behavior_tree_action(const game_behavior_tree_action &other) noexcept;
        game_behavior_tree_action(game_behavior_tree_action &&other) noexcept;
        virtual ~game_behavior_tree_action() noexcept;
        game_behavior_tree_action &operator=(const game_behavior_tree_action &other) noexcept = delete;
        game_behavior_tree_action &operator=(game_behavior_tree_action &&other) noexcept = delete;

        void set_callback(const ft_function<int32_t(game_behavior_context &)> &callback) noexcept;
        const ft_function<int32_t(game_behavior_context &)> &get_callback() const noexcept;

        virtual int32_t tick(game_behavior_context &context) noexcept;
};

class game_behavior_composite : public game_behavior_node
{
    protected:
        ft_vector<ft_sharedptr<game_behavior_node> > _children;
        uint8_t _initialised_state;

        ft_bool validate_child(const ft_sharedptr<game_behavior_node> &child) const noexcept;

    public:
        game_behavior_composite() noexcept;
        game_behavior_composite(const game_behavior_composite &other) noexcept = delete;
        game_behavior_composite(game_behavior_composite &&other) noexcept = delete;
        virtual ~game_behavior_composite() noexcept;
        game_behavior_composite &operator=(const game_behavior_composite &other) noexcept = delete;
        game_behavior_composite &operator=(game_behavior_composite &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_behavior_composite &other) noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        void add_child(const ft_sharedptr<game_behavior_node> &child) noexcept;
        void clear_children() noexcept;
        ft_vector<ft_sharedptr<game_behavior_node> > &get_children() noexcept;
        const ft_vector<ft_sharedptr<game_behavior_node> > &get_children() const noexcept;
};

class game_behavior_selector : public game_behavior_composite
{
    public:
        game_behavior_selector() noexcept;
        game_behavior_selector(const game_behavior_selector &other) noexcept = delete;
        game_behavior_selector(game_behavior_selector &&other) noexcept = delete;
        virtual ~game_behavior_selector() noexcept;
        game_behavior_selector &operator=(const game_behavior_selector &other) noexcept = delete;
        game_behavior_selector &operator=(game_behavior_selector &&other) noexcept = delete;

        virtual int32_t tick(game_behavior_context &context) noexcept;
};

class game_behavior_sequence : public game_behavior_composite
{
    public:
        game_behavior_sequence() noexcept;
        game_behavior_sequence(const game_behavior_sequence &other) noexcept = delete;
        game_behavior_sequence(game_behavior_sequence &&other) noexcept = delete;
        virtual ~game_behavior_sequence() noexcept;
        game_behavior_sequence &operator=(const game_behavior_sequence &other) noexcept = delete;
        game_behavior_sequence &operator=(game_behavior_sequence &&other) noexcept = delete;

        virtual int32_t tick(game_behavior_context &context) noexcept;
};

class game_behavior_tree
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_sharedptr<game_behavior_node> _root;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_behavior_tree() noexcept;
        game_behavior_tree(const game_behavior_tree &other) noexcept = delete;
        game_behavior_tree(game_behavior_tree &&other) noexcept = delete;
        ~game_behavior_tree() noexcept;
        game_behavior_tree &operator=(const game_behavior_tree &other) noexcept = delete;
        game_behavior_tree &operator=(game_behavior_tree &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_behavior_tree &other) noexcept;

        void set_root(const ft_sharedptr<game_behavior_node> &root) noexcept;
        ft_sharedptr<game_behavior_node> &get_root() noexcept;
        const ft_sharedptr<game_behavior_node> &get_root() const noexcept;

        int32_t tick(game_behavior_context &context) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
