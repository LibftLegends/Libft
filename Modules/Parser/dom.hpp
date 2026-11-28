#ifndef PARSER_DOM_HPP
#define PARSER_DOM_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include <stdint.h>

class pt_recursive_mutex;

enum ft_dom_node_type
{
    FT_DOM_NODE_NULL,
    FT_DOM_NODE_OBJECT,
    FT_DOM_NODE_ARRAY,
    FT_DOM_NODE_VALUE,
    FT_DOM_NODE_ELEMENT
};

class ft_dom_node
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_dom_node_type _type;
        ft_string _name;
        ft_string _value;
        ft_vector<ft_dom_node*> _children;
        ft_vector<ft_string> _attribute_keys;
        ft_vector<ft_string> _attribute_values;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        template <typename Text>
        int32_t add_attribute_unlocked(const Text &key, const Text &value) noexcept;
        template <typename Matches>
        ft_dom_node *find_child_if(const Matches &matches) const noexcept;

    public:
        ft_dom_node() noexcept;
        ~ft_dom_node() noexcept;

        ft_dom_node(const ft_dom_node &other) noexcept = delete;
        ft_dom_node &operator=(const ft_dom_node &) = delete;
        ft_dom_node(ft_dom_node &&other) noexcept = delete;
        ft_dom_node &operator=(ft_dom_node &&) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        ft_dom_node_type get_type() const noexcept;
        void set_type(ft_dom_node_type type) noexcept;
        int32_t set_name(const ft_string &name) noexcept;
        int32_t set_name(const char *name) noexcept;
        const ft_string &get_name() const noexcept;
        int32_t set_value(const ft_string &value) noexcept;
        int32_t set_value(const char *value) noexcept;
        const ft_string &get_value() const noexcept;
        int32_t add_child(ft_dom_node *child) noexcept;
        const ft_vector<ft_dom_node*> &get_children() const noexcept;
        int32_t add_attribute(const ft_string &key, const ft_string &value) noexcept;
        int32_t add_attribute(const char *key, const char *value) noexcept;
        ft_bool has_attribute(const ft_string &key) const noexcept;
        ft_string *get_attribute(const ft_string &key) const noexcept;
        const ft_vector<ft_string> &get_attribute_keys() const noexcept;
        const ft_vector<ft_string> &get_attribute_values() const noexcept;
        ft_dom_node *find_child(const ft_string &name) const noexcept;
        ft_dom_node *find_child(const char *name, ft_size_t length) const noexcept;

};

class ft_dom_document
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_dom_node *_root;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:

        ft_dom_document() noexcept;
        ~ft_dom_document() noexcept;

        ft_dom_document(const ft_dom_document &other) noexcept = delete;
        ft_dom_document &operator=(const ft_dom_document &) = delete;
        ft_dom_document(ft_dom_document &&other) noexcept = delete;
        ft_dom_document &operator=(ft_dom_document &&) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        void set_root(ft_dom_node *root) noexcept;
        // Ownership transfers on success.
        int32_t replace_root(ft_dom_node *root) noexcept;
        ft_dom_node *get_root() const noexcept;
        void clear() noexcept;

};

struct ft_dom_schema_rule
{
    ft_string path;
    ft_dom_node_type type;
    ft_bool required;

    ft_dom_schema_rule() noexcept;
    ~ft_dom_schema_rule() noexcept;
    int32_t initialize(const ft_dom_schema_rule &other) noexcept;
    int32_t initialize(ft_dom_schema_rule &&other) noexcept;
    int32_t destroy() noexcept;
};

struct ft_dom_validation_error
{
    ft_string path;
    ft_string message;

    ft_dom_validation_error() noexcept;
    ~ft_dom_validation_error() noexcept;
    int32_t initialize(const ft_dom_validation_error &other) noexcept;
    int32_t destroy() noexcept;
};

class ft_dom_validation_report
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        static thread_local int32_t _last_error;
        ft_bool _valid;
        ft_vector<ft_dom_validation_error> _errors;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        ft_dom_validation_report() noexcept;
        ~ft_dom_validation_report() noexcept;
        ft_dom_validation_report(const ft_dom_validation_report &other) noexcept = delete;
        ft_dom_validation_report &operator=(const ft_dom_validation_report &) = delete;
        ft_dom_validation_report(ft_dom_validation_report &&other) noexcept = delete;
        ft_dom_validation_report &operator=(ft_dom_validation_report &&) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
        int32_t reset() noexcept;

        void mark_valid() noexcept;
        void mark_invalid() noexcept;
        ft_bool valid() const noexcept;
        int32_t add_error(const ft_string &path, const ft_string &message) noexcept;
        const ft_vector<ft_dom_validation_error> &errors() const noexcept;

};

class ft_dom_schema
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        static thread_local int32_t _last_error;
        ft_vector<ft_dom_schema_rule> _rules;
        pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t validate_rule(const ft_dom_schema_rule &rule, const ft_dom_node *node,
            ft_dom_validation_report &report) const noexcept;

    public:
        ft_dom_schema() noexcept;
        ~ft_dom_schema() noexcept;
        ft_dom_schema(const ft_dom_schema &other) noexcept = delete;
        ft_dom_schema &operator=(const ft_dom_schema &) = delete;
        ft_dom_schema(ft_dom_schema &&other) noexcept = delete;
        ft_dom_schema &operator=(ft_dom_schema &&) = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t add_rule(const ft_string &path, ft_dom_node_type type, ft_bool required) noexcept;
        int32_t validate(const ft_dom_document &document, ft_dom_validation_report &report) const noexcept;

};

int32_t ft_dom_find_path(const ft_dom_node *root, const ft_string &path, const ft_dom_node **out_node) noexcept;

#endif
