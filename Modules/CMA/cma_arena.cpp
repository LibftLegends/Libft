#include "cma_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"

#include "../Errno/errno.hpp"
#include <cstdlib>

#define CMA_ARENA_ALLOCATION_MAGIC 0xA4E4A4E4U
#define CMA_ARENA_FREE_MAGIC 0xF4E4F4E4U
#define CMA_SMALL_ARENA_CAPACITY 8388608
#define CMA_SMALL_ARENA_MAX_ALLOCATION 256
#define CMA_SMALL_ARENA_CLASS_COUNT 16

struct cma_arena_allocation_header
{
    uint32_t    magic;
    ft_size_t   size;
};

static cma_arena g_cma_small_arena = {nullptr, 0, 0, FT_FALSE,
    FT_CLASS_STATE_UNINITIALISED};
static ft_size_t g_cma_small_arena_live_count = 0;
static ft_size_t g_cma_small_arena_free_offsets[
    CMA_SMALL_ARENA_CLASS_COUNT] = {FT_SYSTEM_SIZE_MAX};

static void cma_small_arena_clear_free_lists(void)
{
    ft_size_t class_index;

    class_index = 0;
    while (class_index < CMA_SMALL_ARENA_CLASS_COUNT)
    {
        g_cma_small_arena_free_offsets[class_index] = FT_SYSTEM_SIZE_MAX;
        class_index++;
    }
    return ;
}

static ft_size_t cma_small_arena_class_index(ft_size_t size)
{
    ft_size_t class_index;

    if (size == 0 || size > CMA_SMALL_ARENA_MAX_ALLOCATION
        || size % 16 != 0)
        return (FT_SYSTEM_SIZE_MAX);
    class_index = size / 16 - 1;
    if (class_index >= CMA_SMALL_ARENA_CLASS_COUNT)
        return (FT_SYSTEM_SIZE_MAX);
    return (class_index);
}

static ft_size_t cma_arena_align(ft_size_t value, ft_size_t alignment)
{
    ft_size_t remainder;

    if (alignment == 0)
        return (value);
    remainder = value % alignment;
    if (remainder == 0)
        return (value);
    if (value > FT_SYSTEM_SIZE_MAX - (alignment - remainder))
        return (FT_SYSTEM_SIZE_MAX);
    return (value + (alignment - remainder));
}

static ft_bool cma_arena_is_initialised(const cma_arena *arena)
{
    if (arena == nullptr)
        return (FT_FALSE);
    if (arena->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (arena->buffer == nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

static void cma_arena_zero(cma_arena *arena)
{
    if (arena == nullptr)
        return ;
    arena->buffer = nullptr;
    arena->capacity = 0;
    arena->offset = 0;
    arena->owns_buffer = FT_FALSE;
    arena->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

static int32_t cma_arena_initialize(cma_arena *arena, ft_size_t capacity)
{
    uint8_t *buffer;

    if (arena == nullptr || capacity == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (arena->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    buffer = static_cast<uint8_t *>(std::malloc(capacity));
    if (buffer == nullptr)
    {
        cma_arena_zero(arena);
        arena->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    arena->buffer = buffer;
    arena->capacity = capacity;
    arena->offset = 0;
    arena->owns_buffer = FT_TRUE;
    arena->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

static int32_t cma_arena_reset(cma_arena *arena)
{
    if (cma_arena_is_initialised(arena) == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    arena->offset = 0;
    return (FT_ERR_SUCCESS);
}

static cma_arena_allocation_header *cma_arena_header_from_pointer(
        const void *memory_pointer)
{
    uint8_t *payload;

    if (memory_pointer == nullptr)
        return (nullptr);
    payload = static_cast<uint8_t *>(const_cast<void *>(memory_pointer));
    return (reinterpret_cast<cma_arena_allocation_header *>(payload)
        - static_cast<ft_size_t>(1));
}

static ft_bool cma_arena_owns_allocation(const void *memory_pointer,
        void *user_data)
{
    cma_arena *arena;
    cma_arena_allocation_header *header;
    const uint8_t *byte_pointer;

    arena = static_cast<cma_arena *>(user_data);
    if (cma_arena_is_initialised(arena) == FT_FALSE)
        return (FT_FALSE);
    if (memory_pointer == nullptr)
        return (FT_FALSE);
    byte_pointer = static_cast<const uint8_t *>(memory_pointer);
    if (byte_pointer < arena->buffer
        || byte_pointer >= arena->buffer + arena->capacity)
        return (FT_FALSE);
    header = cma_arena_header_from_pointer(memory_pointer);
    if (reinterpret_cast<uint8_t *>(header) < arena->buffer)
        return (FT_FALSE);
    if (header->magic != CMA_ARENA_ALLOCATION_MAGIC)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_size_t cma_arena_get_allocation_size(const void *memory_pointer,
        void *user_data)
{
    cma_arena_allocation_header *header;

    if (cma_arena_owns_allocation(memory_pointer, user_data) == FT_FALSE)
        return (0);
    header = cma_arena_header_from_pointer(memory_pointer);
    return (header->size);
}

static void *cma_arena_allocate_aligned(ft_size_t alignment, ft_size_t size,
        void *user_data)
{
    cma_arena *arena;
    uintptr_t base_address;
    uintptr_t payload_address;
    ft_size_t payload_offset;
    ft_size_t header_offset;
    ft_size_t required_offset;
    cma_arena_allocation_header *header;

    arena = static_cast<cma_arena *>(user_data);
    if (cma_arena_is_initialised(arena) == FT_FALSE)
        return (nullptr);
    if (size == 0)
        size = 1;
    if (alignment < 16)
        alignment = 16;
    if (alignment % 16 != 0)
        return (nullptr);
    if (size > FT_SYSTEM_SIZE_MAX - sizeof(cma_arena_allocation_header))
        return (nullptr);
    base_address = reinterpret_cast<uintptr_t>(arena->buffer);
    payload_address = base_address + arena->offset
        + sizeof(cma_arena_allocation_header);
    payload_address = cma_arena_align(payload_address, alignment);
    if (payload_address < base_address)
        return (nullptr);
    payload_offset = payload_address - base_address;
    if (payload_offset == FT_SYSTEM_SIZE_MAX
        || payload_offset < sizeof(cma_arena_allocation_header))
        return (nullptr);
    header_offset = payload_offset - sizeof(cma_arena_allocation_header);
    if (payload_offset > FT_SYSTEM_SIZE_MAX - size)
        return (nullptr);
    required_offset = payload_offset + size;
    if (required_offset > arena->capacity)
        return (nullptr);
    header = reinterpret_cast<cma_arena_allocation_header *>(
            arena->buffer + header_offset);
    header->magic = CMA_ARENA_ALLOCATION_MAGIC;
    header->size = size;
    arena->offset = required_offset;
    return (arena->buffer + payload_offset);
}

static void *cma_arena_allocate(ft_size_t size, void *user_data)
{
    return (cma_arena_allocate_aligned(16, size, user_data));
}

static void *cma_small_arena_reuse_locked(ft_size_t allocation_size)
{
    ft_size_t class_index;
    ft_size_t payload_offset;
    ft_size_t next_offset;
    uint8_t *payload;
    cma_arena_allocation_header *header;

    class_index = cma_small_arena_class_index(allocation_size);
    if (class_index == FT_SYSTEM_SIZE_MAX)
        return (nullptr);
    payload_offset = g_cma_small_arena_free_offsets[class_index];
    if (payload_offset == FT_SYSTEM_SIZE_MAX)
        return (nullptr);
    if (payload_offset < sizeof(cma_arena_allocation_header)
        || payload_offset > g_cma_small_arena.capacity - allocation_size)
    {
        g_cma_small_arena_free_offsets[class_index] = FT_SYSTEM_SIZE_MAX;
        return (nullptr);
    }
    payload = g_cma_small_arena.buffer + payload_offset;
    header = reinterpret_cast<cma_arena_allocation_header *>(payload)
        - static_cast<ft_size_t>(1);
    if (header->magic != CMA_ARENA_FREE_MAGIC
        || header->size != allocation_size)
    {
        g_cma_small_arena_free_offsets[class_index] = FT_SYSTEM_SIZE_MAX;
        return (nullptr);
    }
    ft_memcpy(&next_offset, payload, sizeof(next_offset));
    if (next_offset != FT_SYSTEM_SIZE_MAX
        && next_offset >= g_cma_small_arena.capacity)
    {
        g_cma_small_arena_free_offsets[class_index] = FT_SYSTEM_SIZE_MAX;
        return (nullptr);
    }
    g_cma_small_arena_free_offsets[class_index] = next_offset;
    header->magic = CMA_ARENA_ALLOCATION_MAGIC;
    return (payload);
}

static void *cma_arena_alloc(cma_arena *arena, ft_size_t size)
{
    return (cma_arena_allocate(size, arena));
}

static void *cma_arena_aligned_alloc(cma_arena *arena, ft_size_t alignment,
        ft_size_t size)
{
    return (cma_arena_allocate_aligned(alignment, size, arena));
}

static void cma_arena_deallocate(void *memory_pointer, void *user_data)
{
    cma_arena_allocation_header *header;
    cma_arena *arena;
    ft_size_t class_index;
    ft_size_t payload_offset;
    ft_size_t next_offset;

    arena = static_cast<cma_arena *>(user_data);
    header = cma_arena_header_from_pointer(memory_pointer);
    if (header == nullptr)
        return ;
    if (header->magic == CMA_ARENA_ALLOCATION_MAGIC)
    {
        class_index = cma_small_arena_class_index(header->size);
        if (arena == &g_cma_small_arena
            && class_index != FT_SYSTEM_SIZE_MAX)
        {
            payload_offset = static_cast<uint8_t *>(memory_pointer)
                - arena->buffer;
            next_offset = g_cma_small_arena_free_offsets[class_index];
            header->magic = CMA_ARENA_FREE_MAGIC;
            ft_memcpy(memory_pointer, &next_offset, sizeof(next_offset));
            g_cma_small_arena_free_offsets[class_index] = payload_offset;
        }
        else
            header->magic = 0;
    }
    return ;
}

static void cma_arena_free(cma_arena *arena, void *memory_pointer)
{
    cma_arena_deallocate(memory_pointer, arena);
    return ;
}

static void *cma_arena_reallocate(void *memory_pointer, ft_size_t size,
        void *user_data)
{
    ft_size_t previous_size;
    ft_size_t copy_size;
    void *new_pointer;

    if (memory_pointer == nullptr)
        return (cma_arena_allocate(size, user_data));
    if (size == 0)
    {
        cma_arena_deallocate(memory_pointer, user_data);
        return (nullptr);
    }
    previous_size = cma_arena_get_allocation_size(memory_pointer, user_data);
    if (previous_size == 0)
        return (nullptr);
    new_pointer = cma_arena_allocate(size, user_data);
    if (new_pointer == nullptr)
        return (nullptr);
    copy_size = previous_size;
    if (copy_size > size)
        copy_size = size;
    ft_memcpy(new_pointer, memory_pointer, copy_size);
    cma_arena_deallocate(memory_pointer, user_data);
    return (new_pointer);
}

static ft_bool cma_small_arena_size_is_supported(ft_size_t size)
{
    if (size == 0)
        size = 1;
    if (size <= CMA_SMALL_ARENA_MAX_ALLOCATION)
        return (FT_TRUE);
    return (FT_FALSE);
}

void cma_small_arena_reset_for_tests(void)
{
    if (g_cma_small_arena.buffer == nullptr)
    {
        g_cma_small_arena_live_count = 0;
        cma_small_arena_clear_free_lists();
        return ;
    }
    if (g_cma_small_arena_live_count != 0)
        return ;
    std::free(g_cma_small_arena.buffer);
    g_cma_small_arena.buffer = nullptr;
    g_cma_small_arena.capacity = 0;
    g_cma_small_arena.offset = 0;
    g_cma_small_arena.owns_buffer = FT_FALSE;
    g_cma_small_arena._initialised_state = FT_CLASS_STATE_UNINITIALISED;
    g_cma_small_arena_live_count = 0;
    cma_small_arena_clear_free_lists();
    return ;
}

static ft_bool cma_small_arena_prepare_locked(void)
{
    int32_t error_code;

    if (cma_arena_is_initialised(&g_cma_small_arena) == FT_TRUE)
        return (FT_TRUE);
    error_code = cma_arena_initialize(&g_cma_small_arena,
            CMA_SMALL_ARENA_CAPACITY);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    g_cma_small_arena_live_count = 0;
    cma_small_arena_clear_free_lists();
    return (FT_TRUE);
}

void *cma_small_arena_allocate_locked(ft_size_t size)
{
    void *memory_pointer;
    ft_size_t allocation_size;

    if (cma_small_arena_size_is_supported(size) == FT_FALSE)
        return (nullptr);
    allocation_size = align16(size);
    if (allocation_size < size)
        return (nullptr);
    if (cma_small_arena_prepare_locked() == FT_FALSE)
        return (nullptr);
    memory_pointer = cma_small_arena_reuse_locked(allocation_size);
    if (memory_pointer == nullptr)
        memory_pointer = cma_arena_alloc(&g_cma_small_arena, allocation_size);
    if (memory_pointer == nullptr && g_cma_small_arena_live_count == 0)
    {
        if (cma_arena_reset(&g_cma_small_arena) != FT_ERR_SUCCESS)
            return (nullptr);
        memory_pointer = cma_arena_alloc(&g_cma_small_arena,
                allocation_size);
    }
    if (memory_pointer == nullptr)
        return (nullptr);
    g_cma_small_arena_live_count++;
    return (memory_pointer);
}

void *cma_small_arena_aligned_allocate_locked(ft_size_t alignment,
        ft_size_t size)
{
    void *memory_pointer;
    ft_size_t allocation_size;

    if (cma_small_arena_size_is_supported(size) == FT_FALSE)
        return (nullptr);
    if (alignment > CMA_SMALL_ARENA_MAX_ALLOCATION)
        return (nullptr);
    allocation_size = align16(size);
    if (allocation_size < size)
        return (nullptr);
    if (cma_small_arena_prepare_locked() == FT_FALSE)
        return (nullptr);
    memory_pointer = cma_arena_aligned_alloc(&g_cma_small_arena, alignment,
            allocation_size);
    if (memory_pointer == nullptr && g_cma_small_arena_live_count == 0)
    {
        if (cma_arena_reset(&g_cma_small_arena) != FT_ERR_SUCCESS)
            return (nullptr);
        memory_pointer = cma_arena_aligned_alloc(&g_cma_small_arena,
                alignment, allocation_size);
    }
    if (memory_pointer == nullptr)
        return (nullptr);
    g_cma_small_arena_live_count++;
    return (memory_pointer);
}

ft_bool cma_small_arena_owns_pointer_locked(const void *memory_pointer)
{
    return (cma_arena_owns_allocation(memory_pointer, &g_cma_small_arena));
}

ft_size_t cma_small_arena_block_size_locked(const void *memory_pointer)
{
    return (cma_arena_get_allocation_size(memory_pointer, &g_cma_small_arena));
}

int32_t cma_small_arena_deallocate_locked(void *memory_pointer)
{
    ft_size_t allocation_size;

    if (cma_small_arena_owns_pointer_locked(memory_pointer) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    allocation_size = cma_small_arena_block_size_locked(memory_pointer);
    cma_arena_free(&g_cma_small_arena, memory_pointer);
    if (g_cma_small_arena_live_count > 0)
        g_cma_small_arena_live_count--;
    if (g_cma_current_bytes >= allocation_size)
        g_cma_current_bytes -= allocation_size;
    else
        g_cma_current_bytes = 0;
    g_cma_free_count++;
    if (g_cma_small_arena_live_count == 0)
    {
        (void)cma_arena_reset(&g_cma_small_arena);
        cma_small_arena_clear_free_lists();
    }
    return (FT_ERR_SUCCESS);
}

void *cma_small_arena_reallocate_locked(void *memory_pointer, ft_size_t size)
{
    void *new_pointer;

    if (cma_small_arena_owns_pointer_locked(memory_pointer) == FT_FALSE)
        return (nullptr);
    if (size > CMA_SMALL_ARENA_MAX_ALLOCATION)
        return (nullptr);
    new_pointer = cma_arena_reallocate(memory_pointer, size,
            &g_cma_small_arena);
    if (new_pointer == nullptr)
        return (nullptr);
    return (new_pointer);
}
