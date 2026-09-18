# Libft Error Code Registry

This registry enumerates every error code defined in [`Errno/errno.hpp`](errno.hpp) and summarizes the modules that are expected to set each value. All codes are relative to `ERRNO_OFFSET` (2000) when mirroring `errno` symbols. Helpers must restore `FT_ERR_SUCCESS` after success paths to avoid leaking stale failures.

| Code | Value | Summary | Primary modules |
| --- | --- | --- | --- |
| `FT_ERR_SUCCESS` | 0 | Operation completed successfully. | All modules |
| `FT_ERR_NO_MEMORY` | 1 | Memory allocation failed or resources were exhausted. | Libft allocation helpers, CMA allocators, containers |
| `FT_ERR_FILE_OPEN_FAILED` | 2 | File open failed. | File module |
| `FT_ERR_INVALID_ARGUMENT` | 3 | Arguments failed validation (null pointers, invalid flags, etc.). | Libft string/memory helpers, System_utils, Networking |
| `FT_ERR_INVALID_POINTER` | 4 | Pointer values were malformed or not owned by the caller. | Libft memory helpers, CMA guard checks |
| `FT_ERR_INVALID_HANDLE` | 5 | Handle-based API received an invalid descriptor/object. | File module, Networking sockets, System_utils handles |
| `FT_ERR_INVALID_OPERATION` | 6 | Requested action violates API preconditions or invariants. | Libft algorithms, CMA stateful helpers |
| `FT_ERR_INVALID_STATE` | 7 | Component was not initialised or is already shut down. | CMA allocator toggles, Logger, Template task utilities |
| `FT_ERR_NOT_FOUND` | 8 | Item lookup failed. | Storage KV store, Template containers, JSON/YAML lookups |
| `FT_ERR_ALREADY_EXISTS` | 9 | Resource already present, preventing creation. | Filesystem helpers, Storage, Networking |
| `FT_ERR_OUT_OF_RANGE` | 10 | Index or numeric value outside accepted range. | Math utilities, Template containers, Libft parsing |
| `FT_ERR_EMPTY` | 11 | Operation requires data but the container/resource is empty. | Template containers, Queue helpers |
| `FT_ERR_FULL` | 12 | Container cannot accept more data. | Template circular buffer, CMA bounded allocators |
| `FT_ERR_IO` | 13 | General I/O failure. | File module, System_utils file helpers, Networking streams |
| `FT_ERR_SYSTEM` | 14 | System-level error propagated from platform `errno`. | System_utils, Compatebility wrappers |
| `FT_ERR_OVERLAP` | 15 | Source and destination buffers overlap when not allowed. | Libft memcpy/memmove safe wrappers |
| `FT_ERR_TERMINATED` | 16 | Operation aborted due to cancellation or shutdown. | Thread pools, Task schedulers, Networking shutdown |
| `FT_ERR_INTERNAL` | 17 | Internal invariant violated (should not happen). | All modules (guarded assertions) |
| `FT_ERR_CONFIGURATION` | 18 | Configuration invalid or missing required fields. | Config module, Logger, Networking setup |
| `FT_ERR_UNSUPPORTED_TYPE` | 19 | Requested type or algorithm not supported. | Serialization helpers, Compression |
| `FT_ERR_ALREADY_INITIALISED` | 20 | Component already initialised. | CMA allocator toggles, Logger, Networking sockets |
| `FT_ERR_INITIALIZATION_FAILED` | 21 | Initialization routine failed. | Logger, Networking, RNG |
| `FT_ERR_END_OF_FILE` | 22 | Reached end-of-file or stream. | File and ReadLine modules |
| `FT_ERR_DIVIDE_BY_ZERO` | 23 | Division by zero attempted. | Math helpers |
| `FT_ERR_BROKEN_PROMISE` | 24 | Promise or future was abandoned. | Template promise/future utilities |
| `FT_ERR_SOCKET_CREATION_FAILED` | 25 | `socket()`/platform equivalent failed. | Networking |
| `FT_ERR_SOCKET_BIND_FAILED` | 26 | Socket bind operation failed. | Networking |
| `FT_ERR_SOCKET_LISTEN_FAILED` | 27 | Socket listen setup failed. | Networking |
| `FT_ERR_SOCKET_CONNECT_FAILED` | 28 | Connect attempt failed. | Networking |
| `FT_ERR_NETWORK_CONNECT_FAILED` | 29 | Network connection failed after transport setup. | Networking higher-level connectors |
| `FT_ERR_INVALID_IP_FORMAT` | 30 | IP address parsing failed. | Networking |
| `FT_ERR_SOCKET_ACCEPT_FAILED` | 31 | `accept()` failed. | Networking |
| `FT_ERR_SOCKET_SEND_FAILED` | 32 | Sending data failed. | Networking |
| `FT_ERR_SOCKET_RECEIVE_FAILED` | 33 | Receiving data failed. | Networking |
| `FT_ERR_SOCKET_CLOSE_FAILED` | 34 | Socket close failed. | Networking |
| `FT_ERR_SOCKET_JOIN_GROUP_FAILED` | 35 | Multicast group join failed. | Networking |
| `FT_ERR_SOCKET_RESOLVE_FAILED` | 36 | Hostname resolution failed (generic). | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_BAD_FLAGS` | 37 | Invalid flags passed to resolver. | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_AGAIN` | 38 | Resolver requested retry (`EAI_AGAIN`). | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_FAIL` | 39 | Non-retriable resolve failure (`EAI_FAIL`). | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_FAMILY` | 40 | Unsupported address family requested. | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_SOCKTYPE` | 41 | Unsupported socket type requested. | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_SERVICE` | 42 | Service/port lookup failed. | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_MEMORY` | 43 | Resolver out of memory. | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_NO_NAME` | 44 | Host/service not found (`EAI_NONAME`). | Networking DNS helpers |
| `FT_ERR_SOCKET_RESOLVE_OVERFLOW` | 45 | Resolver result too large (`EAI_OVERFLOW`). | Networking DNS helpers |
| `FT_ERR_GAME_GENERAL_ERROR` | 46 | Generic game engine failure. | Game module |
| `FT_ERR_GAME_INVALID_MOVE` | 47 | Invalid game move or rule violation. | Game module |
| `FT_ERR_THREAD_BUSY` | 48 | Thread cannot accept additional work. | PThread and Template threading helpers |
| `FT_ERR_MUTEX_NOT_OWNER` | 49 | Mutex unlocked by non-owner. | PThread mutex helpers, Template synchronization |
| `FT_ERR_MUTEX_ALREADY_LOCKED` | 50 | Mutex already locked or recursive violation. | PThread mutex helpers, Template synchronization |
| `FT_ERR_SSL_WANT_READ` | 51 | SSL layer needs more data to read. | Networking TLS wrappers |
| `FT_ERR_SSL_WANT_WRITE` | 52 | SSL layer needs to flush writes. | Networking TLS wrappers |
| `FT_ERR_SSL_ZERO_RETURN` | 53 | SSL connection closed cleanly. | Networking TLS wrappers |
| `FT_ERR_BITSET_NO_MEMORY` | 54 | Bitset allocation failed. | Template bitset |
| `FT_ERR_PRIORITY_QUEUE_EMPTY` | 55 | Priority queue pop on empty container. | Template priority queue |
| `FT_ERR_PRIORITY_QUEUE_NO_MEMORY` | 56 | Priority queue allocation failed. | Template priority queue |
| `FT_ERR_CRYPTO_INVALID_PADDING` | 57 | Cryptographic operation encountered invalid padding. | Encryption module |
| `FT_ERR_DATABASE_UNAVAILABLE` | 58 | Database unavailable. | Database integrations |
| `FT_ERR_TIMEOUT` | 59 | Operation timed out. | Networking timeouts, API clients |
| `FT_ERR_SYS_NO_MEMORY` | 60 | System memory allocation failed. | Errno module (system wrappers) |
| `FT_ERR_SYS_INVALID_STATE` | 61 | System state was invalid. | Errno module (system wrappers) |
| `FT_ERR_SYS_MUTEX_LOCK_FAILED` | 62 | System mutex lock failed. | Errno module (mutex wrappers) |
| `FT_ERR_SYS_MUTEX_ALREADY_LOCKED` | 63 | System mutex was already locked. | Errno module (mutex wrappers) |
| `FT_ERR_SYS_MUTEX_NOT_OWNER` | 64 | System mutex unlock attempted by non-owner. | Errno module (mutex wrappers) |
| `FT_ERR_SYS_MUTEX_UNLOCK_FAILED` | 65 | System mutex unlock failed. | Errno module (mutex wrappers) |
| `FT_ERR_SYS_INTERNAL` | 66 | Internal system error. | Errno module (system wrappers) |
| `FT_ERR_RWLOCK_READER_CAPACITY` | 74 | Thread-local RW-lock reader ownership capacity reached. | PThread optimized RW-lock |
| `FT_ERR_SSL_SYSCALL_ERROR` | 2005 | SSL syscall failure propagated from platform `errno`. | Networking TLS wrappers |
| `FT_ERR_HTTP_PROTOCOL_MISMATCH` | 2006 | Server replied with a different HTTP version than requested. | API HTTP clients |
| `FT_ERR_API_CIRCUIT_OPEN` | 2007 | API circuit breaker prevented request execution. | API HTTP clients |

## Usage guidance

* When adding new error codes, update this registry and provide guidance on the modules that surface the value.
* Modules should document code-specific remediation steps in their README entries and keep error propagation consistent with this table.
* Tests should exercise both success and failure paths to ensure error codes are pushed and cleared as described here.
