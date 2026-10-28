#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "encryption.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t be_default_open(const char *path_name, int32_t flags, mode_t mode)
{
    return (su_open(path_name, flags, mode));
}

static int64_t be_default_write(int32_t file_descriptor, const void *buffer,
    ft_size_t count)
{
    return (su_write(file_descriptor, buffer, count));
}

#ifdef LIBFT_TEST_BUILD
static t_be_open_function g_be_open_function = be_default_open;
static t_be_write_function g_be_write_function = be_default_write;

void be_set_save_game_hooks(t_be_open_function open_function, t_be_write_function write_function)
{
    if (open_function != ft_nullptr)
        g_be_open_function = open_function;
    else
        g_be_open_function = be_default_open;
    if (write_function != ft_nullptr)
        g_be_write_function = write_function;
    else
        g_be_write_function = be_default_write;
    return ;
}
#endif

static void be_encrypt(char *data, ft_size_t data_length, const char *key)
{
    uint32_t hash = 5381;
    ft_size_t key_length = ft_strlen(key);
    ft_size_t key_index = 0;
    while (key_index < key_length)
    {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(key[key_index]);
        ++key_index;
    }
    ft_size_t data_index = 0;
    while (data_index < data_length)
    {
        data[data_index] ^= static_cast<char>((hash >> (data_index % 8)) & 0xFF);
        ++data_index;
    }
    return ;
}

int32_t be_save_game(const char *filename, const char *data, const char *key)
{
    ft_size_t data_length = ft_strlen(data);
    char *encrypted_data = static_cast<char *>(cma_malloc(data_length));
    if (encrypted_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_memcpy(encrypted_data, data, data_length);
    be_encrypt(encrypted_data, data_length, key);
    int32_t file_descriptor;

#ifdef LIBFT_TEST_BUILD
    file_descriptor = g_be_open_function(filename,
            O_WRONLY | O_CREAT | O_TRUNC, 0644);
#else
    file_descriptor = be_default_open(filename, O_WRONLY | O_CREAT | O_TRUNC,
            0644);
#endif
    if (file_descriptor < 0)
    {
        cma_free(encrypted_data);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    int64_t bytes_written;

#ifdef LIBFT_TEST_BUILD
    bytes_written = g_be_write_function(file_descriptor, encrypted_data,
            data_length);
#else
    bytes_written = be_default_write(file_descriptor, encrypted_data,
            data_length);
#endif
    cmp_close(file_descriptor);
    cma_free(encrypted_data);
    if (bytes_written == static_cast<int64_t>(data_length))
        return (FT_ERR_SUCCESS);
    return (FT_ERR_IO);
}

char **be_decrypt_data(char **data, const char *key)
{
    if (data == ft_nullptr || *data == ft_nullptr)
        return (ft_nullptr);
    ft_size_t data_length = ft_strlen(*data);
    be_encrypt(*data, data_length, key);
    return (data);
}
