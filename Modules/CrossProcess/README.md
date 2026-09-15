# CrossProcess

The `CrossProcess` module moves descriptors and memory payload metadata between processes. It is intended for socket-based coordination where one process shares memory and another process receives enough information to read or write that memory. Descriptor messages use a fixed 304-byte, versioned, big-endian wire format; they never expose the C++ structure layout, padding, or host endianness.

## Types

- `cross_process_message` - Descriptor payload containing stack base, remote memory address/size, shared mutex and error-memory addresses, plus a fixed shared-memory name buffer.
- `cross_process_read_result` - Result of receiving shared memory. It contains an `ft_string` for the shared-memory name and an `ft_string` for the payload.

## Public API

- `cp_send_descriptor(int32_t socket_file_descriptor, const cross_process_message &message)` - Sends a descriptor message over a socket.
- `cp_receive_descriptor(int32_t socket_file_descriptor, cross_process_message &message)` - Receives a descriptor message from a socket.
- `cp_receive_memory(int32_t socket_file_descriptor, cross_process_read_result &result)` - Receives shared-memory metadata and loads the payload into `result`.
- `cp_write_memory(const cross_process_message &message, const uint8_t *payload, ft_size_t payload_length, int32_t error_code)` - Writes a payload and error code into the remote/shared memory described by a message.

Descriptor validation rejects zero mappings, unterminated shared-memory names,
addresses below the advertised mapping base, and offsets outside the mapping.
POSIX receivers verify the backing object's size before mapping it. Descriptor
decoding is transactional: malformed or unsupported wire data leaves the
caller-provided message unchanged.

`cross_process_read_result::consumed` is set only after the payload and error
slot have been cleared. If a later unlock or unmap operation fails, the
function still returns that cleanup error but `consumed` remains true, so a
caller will not incorrectly retry a payload that has already been consumed.
Owner-death/abandoned-mutex acquisition is reported as `FT_ERR_INVALID_STATE`
after the platform mutex is made unlockable; callers must discard and rebuild
the affected shared-memory operation rather than trusting partially committed
contents.
