#ifndef BYTE_BUFFER_HPP
# define BYTE_BUFFER_HPP

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <cstdint>

class ft_byte_buffer
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        static thread_local int32_t _last_error;
        uint8_t *_data;
        ft_size_t _size;
        ft_size_t _capacity;
        ft_size_t _read_position;
        ft_bool _fixed_capacity;
        uint8_t _initialised_state;
        pt_recursive_mutex *_mutex;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t reserve_internal(ft_size_t required_capacity) noexcept;
        int32_t append_internal(const void *data, ft_size_t length) noexcept;
        int32_t read_internal(void *data, ft_size_t length) noexcept;
        int32_t lock_internal(void) const noexcept;
        void unlock_internal(void) const noexcept;
        void reset_fields(void) noexcept;

    public:
        ft_byte_buffer() noexcept;
        ft_byte_buffer(const ft_byte_buffer &other) noexcept = delete;
        ft_byte_buffer(ft_byte_buffer &&other) noexcept = delete;
        ~ft_byte_buffer() noexcept;

        ft_byte_buffer &operator=(const ft_byte_buffer &other) noexcept = delete;
        ft_byte_buffer &operator=(ft_byte_buffer &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(ft_size_t capacity, ft_bool fixed_capacity) noexcept;
        // The reader must fill exactly length bytes on success and must not re-enter the buffer.
        // Zero length skips the reader. Success leaves a fixed-capacity buffer.
        int32_t initialize_from_reader(ft_size_t length,
            int32_t (*reader)(uint8_t *destination, ft_size_t length, void *user_data) noexcept,
            void *user_data) noexcept;
        int32_t initialize(const ft_byte_buffer &other) noexcept;
        int32_t initialize(ft_byte_buffer &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_byte_buffer &other) noexcept;

        int32_t reserve(ft_size_t required_capacity) noexcept;
        int32_t clear() noexcept;
        int32_t reset_read_position() noexcept;
        int32_t set_read_position(ft_size_t position) noexcept;
        int32_t consume(ft_size_t length) noexcept;
        int32_t append(const void *data, ft_size_t length) noexcept;
        int32_t append_buffer(const ft_byte_buffer &other) noexcept;
        int32_t prepend_buffer(const ft_byte_buffer &other) noexcept;
        int32_t read(void *data, ft_size_t length) noexcept;
        int32_t view(ft_size_t offset, ft_size_t length,
            const uint8_t **view_out) const noexcept;
        int32_t shrink_to_fit() noexcept;

        int32_t append_u8(uint8_t value) noexcept;
        int32_t append_u16_be(uint16_t value) noexcept;
        int32_t append_u16_le(uint16_t value) noexcept;
        int32_t append_u32_be(uint32_t value) noexcept;
        int32_t append_u32_le(uint32_t value) noexcept;
        int32_t overwrite_u32_le(ft_size_t offset, uint32_t value) noexcept;
        int32_t append_u64_be(uint64_t value) noexcept;
        int32_t append_u64_le(uint64_t value) noexcept;
        int32_t append_varuint64(uint64_t value) noexcept;
        int32_t append_varint64(int64_t value) noexcept;

        int32_t read_u8(uint8_t *value_out) noexcept;
        int32_t read_u16_be(uint16_t *value_out) noexcept;
        int32_t read_u16_le(uint16_t *value_out) noexcept;
        int32_t read_u32_be(uint32_t *value_out) noexcept;
        int32_t read_u32_le(uint32_t *value_out) noexcept;
        int32_t read_u64_be(uint64_t *value_out) noexcept;
        int32_t read_u64_le(uint64_t *value_out) noexcept;
        int32_t read_varuint64(uint64_t *value_out) noexcept;
        int32_t read_varint64(int64_t *value_out) noexcept;

        ft_size_t find(const void *data, ft_size_t length) const noexcept;
        int32_t peek(ft_size_t offset, void *data, ft_size_t length) const noexcept;
        int32_t skip(ft_size_t length) noexcept;
        int32_t slice(ft_size_t offset, ft_size_t length, ft_byte_buffer &output) const noexcept;

        const uint8_t *data() const noexcept;
        ft_size_t size() const noexcept;
        ft_size_t capacity() const noexcept;
        ft_size_t read_position() const noexcept;
        ft_size_t remaining() const noexcept;
        ft_bool is_fixed_capacity() const noexcept;
        ft_bool is_initialised() const noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
