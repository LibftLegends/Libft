#include <cstring>
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
    unsigned char *heap_data;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (request_count != 0 && requests == ft_nullptr)
        return (scma_unlock_and_return_int(FT_ERR_INVALID_POINTER));
    request_index = 0;
    while (request_index < request_count)
    {
        validation_result = scma_validate_write_request(
                &requests[request_index], &block);
        if (validation_result != FT_ERR_SUCCESS)
            return (scma_unlock_and_return_int(validation_result));
        request_index++;
    }
    heap_data = scma_get_heap_data();
    request_index = 0;
    while (request_index < request_count)
    {
        (void)scma_validate_handle(requests[request_index].handle, &block);
        std::memcpy(heap_data + block->offset + requests[request_index].offset,
            requests[request_index].source, requests[request_index].size);
        request_index++;
    }
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t    scma_read_batch(const scma_read_request *requests,
        ft_size_t request_count)
{
    ft_size_t request_index;
    int32_t validation_result;
    scma_block *block;
    unsigned char *heap_data;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (request_count != 0 && requests == ft_nullptr)
        return (scma_unlock_and_return_int(FT_ERR_INVALID_POINTER));
    request_index = 0;
    while (request_index < request_count)
    {
        validation_result = scma_validate_read_request(
                &requests[request_index], &block);
        if (validation_result != FT_ERR_SUCCESS)
            return (scma_unlock_and_return_int(validation_result));
        request_index++;
    }
    heap_data = scma_get_heap_data();
    request_index = 0;
    while (request_index < request_count)
    {
        (void)scma_validate_handle(requests[request_index].handle, &block);
        std::memcpy(requests[request_index].destination,
            heap_data + block->offset + requests[request_index].offset,
            requests[request_index].size);
        request_index++;
    }
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}
