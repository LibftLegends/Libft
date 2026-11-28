#ifndef DOCUMENT_BACKEND_HPP
#define DOCUMENT_BACKEND_HPP

#include <cstddef>
#include "../CPP_class/class_string.hpp"
#include "../Networking/openssl_support.hpp"
#include <stdint.h>

class pt_recursive_mutex;

class ft_document_source
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        pt_recursive_mutex *_mutex;
#ifndef LIBFT_TEST_BUILD
    protected:
#endif
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        ft_document_source() noexcept;
        virtual ~ft_document_source();
        ft_document_source(const ft_document_source &other) noexcept = delete;
        ft_document_source &operator=(const ft_document_source &other) = delete;
        ft_document_source(ft_document_source &&other) noexcept = delete;
        ft_document_source &operator=(ft_document_source &&other) = delete;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;
        virtual int32_t read_all(ft_string &output) = 0;
        virtual int32_t initialize() noexcept;
        virtual int32_t destroy() noexcept;

};

class ft_document_sink
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        pt_recursive_mutex *_mutex;
#ifndef LIBFT_TEST_BUILD
    protected:
#endif
        uint8_t _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        ft_document_sink() noexcept;
        virtual ~ft_document_sink();
        ft_document_sink(const ft_document_sink &other) noexcept = delete;
        ft_document_sink &operator=(const ft_document_sink &other) = delete;
        ft_document_sink(ft_document_sink &&other) noexcept = delete;
        ft_document_sink &operator=(ft_document_sink &&other) = delete;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;
        virtual int32_t write_all(const char *data_pointer, ft_size_t data_length) = 0;
        virtual int32_t initialize() noexcept;
        virtual int32_t destroy() noexcept;

};

class ft_file_document_source : public ft_document_source
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _path;

    public:
        ft_file_document_source() noexcept;
        ft_file_document_source(const ft_file_document_source &other) noexcept = delete;
        ft_file_document_source(ft_file_document_source &&other) noexcept = delete;
        virtual ~ft_file_document_source();
        
        ft_file_document_source &operator=(const ft_file_document_source &other) = delete;
        
        ft_file_document_source &operator=(ft_file_document_source &&other) = delete;

        int32_t initialize() noexcept override;
        int32_t destroy() noexcept override;

        void set_path(const char *file_path) noexcept;
        const char *get_path() const noexcept;
        virtual int32_t read_all(ft_string &output) override;

};

class ft_file_document_sink : public ft_document_sink
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _path;

    public:
        ft_file_document_sink() noexcept;
        ft_file_document_sink(const ft_file_document_sink &other) noexcept = delete;
        ft_file_document_sink(ft_file_document_sink &&other) noexcept = delete;
        virtual ~ft_file_document_sink();
        
        ft_file_document_sink &operator=(const ft_file_document_sink &other) = delete;
        
        ft_file_document_sink &operator=(ft_file_document_sink &&other) = delete;

        int32_t initialize() noexcept override;
        int32_t destroy() noexcept override;

        void set_path(const char *file_path) noexcept;
        const char *get_path() const noexcept;
        virtual int32_t write_all(const char *data_pointer, ft_size_t data_length) override;

};

class ft_memory_document_source : public ft_document_source
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        const char *_data_pointer;
        ft_size_t _data_length;

    public:
        ft_memory_document_source() noexcept;
        ft_memory_document_source(const ft_memory_document_source &other) noexcept = delete;
        ft_memory_document_source(ft_memory_document_source &&other) noexcept = delete;
        virtual ~ft_memory_document_source();
        
        ft_memory_document_source &operator=(const ft_memory_document_source &other) = delete;
        
        ft_memory_document_source &operator=(ft_memory_document_source &&other) = delete;

        int32_t destroy() noexcept override;

        void set_data(const char *data_pointer, ft_size_t data_length) noexcept;
        const char *get_data() const noexcept;
        ft_size_t get_length() const noexcept;
        virtual int32_t read_all(ft_string &output) override;

};

class ft_memory_document_sink : public ft_document_sink
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string *_storage_pointer;

    public:
        ft_memory_document_sink() noexcept;
        ft_memory_document_sink(const ft_memory_document_sink &other) noexcept = delete;
        ft_memory_document_sink(ft_memory_document_sink &&other) noexcept = delete;
        virtual ~ft_memory_document_sink();
        
        ft_memory_document_sink &operator=(const ft_memory_document_sink &other) = delete;
        
        ft_memory_document_sink &operator=(ft_memory_document_sink &&other) = delete;

        int32_t destroy() noexcept override;

        void set_storage(ft_string *storage_pointer) noexcept;
        ft_string *get_storage() const noexcept;
        virtual int32_t write_all(const char *data_pointer, ft_size_t data_length) override;

};

#if NETWORKING_HAS_OPENSSL
class ft_http_document_source : public ft_document_source
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _host;
        ft_string _path;
        ft_string _port;
        ft_bool _use_ssl;

    public:
        ft_http_document_source() noexcept;
        ft_http_document_source(const ft_http_document_source &other) noexcept = delete;
        ft_http_document_source(ft_http_document_source &&other) noexcept = delete;
        virtual ~ft_http_document_source();
        
        ft_http_document_source &operator=(const ft_http_document_source &other) = delete;
        
        ft_http_document_source &operator=(ft_http_document_source &&other) = delete;

        int32_t initialize() noexcept override;
        int32_t destroy() noexcept override;

        // A failed configuration update leaves the previous endpoint intact.
        void configure(const char *host, const char *path, ft_bool use_ssl, const char *port) noexcept;
        const char *get_host() const noexcept;
        const char *get_path() const noexcept;
        const char *get_port() const noexcept;
        ft_bool is_ssl_enabled() const noexcept;
        virtual int32_t read_all(ft_string &output) override;

};

class ft_http_document_sink : public ft_document_sink
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _host;
        ft_string _path;
        ft_string _port;
        ft_bool _use_ssl;

    public:
        ft_http_document_sink() noexcept;
        ft_http_document_sink(const ft_http_document_sink &other) noexcept = delete;
        ft_http_document_sink(ft_http_document_sink &&other) noexcept = delete;
        virtual ~ft_http_document_sink();
        
        ft_http_document_sink &operator=(const ft_http_document_sink &other) = delete;
        
        ft_http_document_sink &operator=(ft_http_document_sink &&other) = delete;

        int32_t initialize() noexcept override;
        int32_t destroy() noexcept override;

        // A failed configuration update leaves the previous endpoint intact.
        void configure(const char *host, const char *path, ft_bool use_ssl, const char *port) noexcept;
        const char *get_host() const noexcept;
        const char *get_path() const noexcept;
        const char *get_port() const noexcept;
        ft_bool is_ssl_enabled() const noexcept;
        virtual int32_t write_all(const char *data_pointer, ft_size_t data_length) override;

};
#endif

#endif
