#ifndef COMPATEBILITY_CROSS_PROCESS_HPP
#define COMPATEBILITY_CROSS_PROCESS_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../CrossProcess/cross_process.hpp"

struct cmp_cross_process_mapping
{
    unsigned char *mapping_address;
    ft_size_t mapping_length;
    void *platform_handle;
    void *mutex_address;
};

struct cmp_cross_process_mutex_state
{
    void *platform_mutex;
    ft_bool owner_recovered;
};

static constexpr uint32_t CMP_CROSS_PROCESS_WIRE_MAGIC = 0x43505831U;
static constexpr uint16_t CMP_CROSS_PROCESS_WIRE_VERSION = 1U;
static constexpr ft_size_t CMP_CROSS_PROCESS_WIRE_SIZE = 304U;

int32_t cmp_cross_process_encode_wire(const cross_process_message &message,
    uint8_t *wire, ft_size_t wire_size);
int32_t cmp_cross_process_decode_wire(const uint8_t *wire,
    ft_size_t wire_size, cross_process_message &message);

int32_t cmp_cross_process_send_descriptor(int32_t socket_file_descriptor, const cross_process_message &message);
int32_t cmp_cross_process_receive_descriptor(int32_t socket_file_descriptor, cross_process_message &message);
int32_t cmp_cross_process_open_mapping(const cross_process_message &message, cmp_cross_process_mapping *mapping);
int32_t cmp_cross_process_close_mapping(cmp_cross_process_mapping *mapping);
int32_t cmp_cross_process_lock_mutex(const cross_process_message &message, cmp_cross_process_mapping *mapping, cmp_cross_process_mutex_state *mutex_state);
int32_t cmp_cross_process_unlock_mutex(const cross_process_message &message, cmp_cross_process_mapping *mapping, cmp_cross_process_mutex_state *mutex_state);

#if defined(LIBFT_TEST_BUILD)
int32_t cmp_cross_process_test_fail_next_unlock(void);
#endif

#endif
