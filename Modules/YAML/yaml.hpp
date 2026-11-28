#ifndef YAML_HPP
#define YAML_HPP

#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Template/map.hpp"
#include "../Parser/document_backend.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <cstdint>

enum yaml_type
{
    YAML_SCALAR,
    YAML_LIST,
    YAML_MAP
};

class yaml_value
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        yaml_type _type;
        ft_string _scalar;
        ft_vector<yaml_value*> _list;
        ft_map<ft_string, yaml_value*> _map;
        ft_vector<ft_string> _map_keys;

        mutable pt_recursive_mutex *_mutex;

        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

    public:
        yaml_value() noexcept;
        yaml_value(const yaml_value &other) noexcept = delete;
        yaml_value(yaml_value &&other) noexcept = delete;
        ~yaml_value() noexcept;

        yaml_value &operator=(const yaml_value &) = delete;
        yaml_value &operator=(yaml_value &&) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const yaml_value &other) noexcept;
        int32_t initialize(yaml_value &&other) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(yaml_value &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        void set_type(yaml_type type) noexcept;
        yaml_type get_type() const noexcept;

        void set_scalar(const ft_string &value) noexcept;
        const ft_string &get_scalar() const noexcept;

        void add_list_item(yaml_value *item) noexcept;
        const ft_vector<yaml_value*> &get_list() const noexcept;

        void add_map_item(const ft_string &key, yaml_value *value) noexcept;
        const ft_map<ft_string, yaml_value*> &get_map() const noexcept;
        const ft_vector<ft_string> &get_map_keys() const noexcept;
};

ft_size_t      yaml_find_char(const ft_string &string, char character) noexcept;
ft_string   *yaml_substr(const ft_string &string, ft_size_t start, ft_size_t length) noexcept;
ft_string   *yaml_substr_from(const ft_string &string, ft_size_t start) noexcept;
ft_size_t      yaml_count_indent(const ft_string &line) noexcept;
void        yaml_trim(ft_string &string) noexcept;
int32_t         yaml_split_lines(const ft_string &content, ft_vector<ft_string> &lines) noexcept;

yaml_value    *yaml_read_from_string(const ft_string &content) noexcept;
yaml_value    *yaml_read_from_file(const char *file_path) noexcept;
yaml_value    *yaml_read_from_backend(ft_document_source &source) noexcept;
ft_string     *yaml_write_to_string(const yaml_value *value) noexcept;
int32_t        yaml_write_to_string(const yaml_value *value, ft_string &output) noexcept;
int32_t           yaml_write_to_file(const char *file_path, const yaml_value *value) noexcept;
int32_t           yaml_write_to_backend(ft_document_sink &sink, const yaml_value *value) noexcept;
void          yaml_free(yaml_value *value) noexcept;

typedef int32_t (*yaml_serialize_callback)(
    yaml_value &root, void *user_data) noexcept;
typedef int32_t (*yaml_deserialize_callback)(
    const yaml_value &root, void *user_data) noexcept;

int32_t yaml_serialize_to_string(yaml_serialize_callback serialize_callback,
    void *user_data, ft_string &output) noexcept;
int32_t yaml_serialize_to_backend(yaml_serialize_callback serialize_callback,
    void *user_data, ft_document_sink &sink) noexcept;
int32_t yaml_deserialize_from_string(const ft_string &content,
    yaml_deserialize_callback deserialize_callback, void *user_data) noexcept;
int32_t yaml_deserialize_from_backend(ft_document_source &source,
    yaml_deserialize_callback deserialize_callback, void *user_data) noexcept;

#endif
