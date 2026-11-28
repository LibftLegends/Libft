#ifndef CROSS_PROCESS_HPP
#define CROSS_PROCESS_HPP

#include <cstdint>

#include "../CPP_class/class_string.hpp"
#include "../Basic/basic.hpp"

struct cross_process_message
{
    uint64_t stack_base_address;
    uint64_t remote_memory_address;
    uint64_t remote_memory_size;
    uint64_t shared_mutex_address;
    uint64_t error_memory_address;
    char shared_memory_name[256];
};

struct cross_process_read_result
{
    ft_string shared_memory_name;
    ft_string payload;
    ft_bool consumed;
};

int32_t cp_send_descriptor(int32_t socket_file_descriptor,
    const cross_process_message &message);
int32_t cp_receive_descriptor(int32_t socket_file_descriptor,
    cross_process_message &message);
int32_t cp_receive_memory(int32_t socket_file_descriptor,
    cross_process_read_result &result);
int32_t cp_write_memory(const cross_process_message &message,
    const uint8_t *payload, ft_size_t payload_length, int32_t error_code);

#endif
