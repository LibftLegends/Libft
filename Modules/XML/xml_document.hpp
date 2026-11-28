#ifndef XML_DOCUMENT_HPP
#define XML_DOCUMENT_HPP

#include "../Template/vector.hpp"
#include "../Template/unordered_map.hpp"
#include "../Errno/errno.hpp"
#include "../CMA/CMA.hpp"
#include "../Parser/document_backend.hpp"
#include <cstdint>

class pt_recursive_mutex;
class ft_dom_document;

struct xml_namespace_entry;

struct xml_node
{
    pt_recursive_mutex *mutex;
    char *name;
    char *namespace_prefix;
    char *local_name;
    const char *namespace_uri;
    xml_namespace_entry *namespace_bindings;
    char *text;
    ft_vector<xml_node*> children;
    ft_unordered_map<char*, char*> attributes;

    xml_node() noexcept;
    ~xml_node() noexcept;

    xml_node(const xml_node &other) noexcept = delete;
    xml_node &operator=(const xml_node &other) noexcept = delete;
    xml_node(xml_node &&other) noexcept = delete;
    xml_node &operator=(xml_node &&other) noexcept = delete;
};

class xml_document
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        xml_node *_root;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        void record_operation_error(int32_t error_code) const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;
        int32_t serialize(ft_string &buffer) const noexcept;
        int32_t replace_from_dom(const ft_dom_document &dom) noexcept;
        friend int32_t xml_document_to_dom(const xml_document &document, ft_dom_document &dom) noexcept;
        friend int32_t xml_document_from_dom(const ft_dom_document &dom, xml_document &document) noexcept;

    public:
        xml_document() noexcept;
        xml_document(const xml_document &other) noexcept = delete;
        xml_document(xml_document &&other) noexcept = delete;
        ~xml_document() noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t initialize() noexcept;
        int32_t initialize(const xml_document &other) noexcept;
        int32_t initialize(xml_document &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(xml_document &other) noexcept;
        int32_t load_from_string(const char *xml) noexcept;
        int32_t load_from_file(const char *file_path) noexcept;
        int32_t load_from_backend(ft_document_source &source) noexcept;
        char *write_to_string() const noexcept;
        int32_t write_to_string(ft_string &output) const noexcept;
        int32_t write_to_file(const char *file_path) const noexcept;
        int32_t write_to_backend(ft_document_sink &sink) const noexcept;
        xml_node *get_root() const noexcept;
        void set_manual_error(int32_t error_code) noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        xml_document &operator=(const xml_document &other) noexcept = delete;
        xml_document &operator=(xml_document &&other) noexcept = delete;
};

int32_t  xml_node_prepare_thread_safety(xml_node *node) noexcept;
void xml_node_teardown_thread_safety(xml_node *node) noexcept;
int32_t  xml_node_lock(const xml_node *node, ft_bool *lock_acquired) noexcept;
void xml_node_unlock(const xml_node *node, ft_bool lock_acquired) noexcept;

#endif
