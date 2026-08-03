#include <cstring>
#include <cstdlib>
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "SCMA.hpp"
#include "scma_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t    scma_validate_write_request(
        const scma_write_request *request, scma_block **block)
{
    if (!scma_validate_handle(request->handle, block))
        return (FT_ERR_INVALID_HANDLE);
    if (!request->source)
        return (FT_ERR_INVALID_POINTER);
    if (request->offset > (*block)->size)
        return (FT_ERR_OUT_OF_RANGE);
    if (request->size > (*block)->size - request->offset)
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}

static int32_t    scma_validate_read_request(
        const scma_read_request *request, scma_block **block)
{
    if (!scma_validate_handle(request->handle, block))
        return (FT_ERR_INVALID_HANDLE);
    if (!request->destination)
        return (FT_ERR_INVALID_POINTER);
    if (request->offset > (*block)->size)
        return (FT_ERR_OUT_OF_RANGE);
    if (request->size > (*block)->size - request->offset)
        return (FT_ERR_OUT_OF_RANGE);
    return (FT_ERR_SUCCESS);
}

int32_t    scma_write(scma_handle handle, ft_size_t offset,
                const void *source, ft_size_t size)
{
    scma_write_request request;

    request.handle = handle;
    request.offset = offset;
    request.source = source;
    request.size = size;
    return (scma_write_batch(&request, 1));
}

int32_t    scma_read(scma_handle handle, ft_size_t offset,
                void *destination, ft_size_t size)
{
    scma_read_request request;

    request.handle = handle;
    request.offset = offset;
    request.destination = destination;
    request.size = size;
    return (scma_read_batch(&request, 1));
}

int32_t    scma_write_batch(const scma_write_request *requests,
        ft_size_t request_count)
{
    ft_size_t request_index;
    int32_t validation_result;
    scma_block *block;
    scma_block **validated_blocks;
    unsigned char *heap_data;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (request_count != 0 && requests == ft_nullptr)
        return (scma_unlock_and_return_int(FT_ERR_INVALID_POINTER));
    validated_blocks = ft_nullptr;
    if (request_count != 0)
    {
        if (request_count > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                / sizeof(scma_block *))
            return (scma_unlock_and_return_int(FT_ERR_OUT_OF_RANGE));
        validated_blocks = static_cast<scma_block **>(std::malloc(
                    request_count * sizeof(scma_block *)));
        if (validated_blocks == ft_nullptr)
            return (scma_unlock_and_return_int(FT_ERR_NO_MEMORY));
    }
    request_index = 0;
    while (request_index < request_count)
    {
        validation_result = scma_validate_write_request(
                &requests[request_index], &block);
        if (validation_result != FT_ERR_SUCCESS)
        {
            std::free(validated_blocks);
            return (scma_unlock_and_return_int(validation_result));
        }
        validated_blocks[request_index] = block;
        request_index++;
    }
    heap_data = scma_get_heap_data();
    request_index = 0;
    while (request_index < request_count)
    {
        block = validated_blocks[request_index];
        std::memmove(heap_data + block->offset + requests[request_index].offset,
            requests[request_index].source, requests[request_index].size);
        request_index++;
    }
    std::free(validated_blocks);
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t    scma_read_batch(const scma_read_request *requests,
        ft_size_t request_count)
{
    ft_size_t request_index;
    int32_t validation_result;
    scma_block *block;
    scma_block **validated_blocks;
    unsigned char *heap_data;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (request_count != 0 && requests == ft_nullptr)
        return (scma_unlock_and_return_int(FT_ERR_INVALID_POINTER));
    validated_blocks = ft_nullptr;
    if (request_count != 0)
    {
        if (request_count > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                / sizeof(scma_block *))
            return (scma_unlock_and_return_int(FT_ERR_OUT_OF_RANGE));
        validated_blocks = static_cast<scma_block **>(std::malloc(
                    request_count * sizeof(scma_block *)));
        if (validated_blocks == ft_nullptr)
            return (scma_unlock_and_return_int(FT_ERR_NO_MEMORY));
    }
    request_index = 0;
    while (request_index < request_count)
    {
        validation_result = scma_validate_read_request(
                &requests[request_index], &block);
        if (validation_result != FT_ERR_SUCCESS)
        {
            std::free(validated_blocks);
            return (scma_unlock_and_return_int(validation_result));
        }
        validated_blocks[request_index] = block;
        request_index++;
    }
    heap_data = scma_get_heap_data();
    request_index = 0;
    while (request_index < request_count)
    {
        block = validated_blocks[request_index];
        std::memmove(requests[request_index].destination,
            heap_data + block->offset + requests[request_index].offset,
            requests[request_index].size);
        request_index++;
    }
    std::free(validated_blocks);
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}
