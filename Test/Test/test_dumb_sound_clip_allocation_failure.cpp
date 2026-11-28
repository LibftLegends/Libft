#include "../test_internal.hpp"
#include "../../Modules/DUMB/sound_clip.hpp"
#include "../../Modules/DUMB/sound_device.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#if defined(__has_include)
#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#define FT_HAS_VALGRIND_HEADER 1
#else
#define FT_HAS_VALGRIND_HEADER 0
#endif
#else
#define FT_HAS_VALGRIND_HEADER 0
#endif

#ifndef LIBFT_TEST_BUILD
#endif

static int32_t dumb_prepare_sparse_wav_file(char *path_buffer, ft_size_t path_buffer_size)
{
    int32_t file_descriptor;
    int32_t truncate_status;

    if (path_buffer == ft_nullptr || path_buffer_size < 32)
        return (0);
    std::snprintf(path_buffer, path_buffer_size, "/tmp/libft_dumb_wav_XXXXXX");
    file_descriptor = test_create_temp_file_from_template(path_buffer,
            path_buffer_size, path_buffer);
    if (file_descriptor < 0)
        return (0);
    constexpr uint8_t PCM_HEADER[] = {
        'R', 'I', 'F', 'F', 36, 0, 0, 1, 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
        68, 172, 0, 0, 136, 88, 1, 0, 2, 0, 16, 0,
        'd', 'a', 't', 'a', 0, 0, 0, 1
    };
    truncate_status = -1;
    const int64_t written = write(file_descriptor, PCM_HEADER, sizeof(PCM_HEADER));
    if (written == static_cast<int64_t>(sizeof(PCM_HEADER)))
        truncate_status = ftruncate(file_descriptor,
            static_cast<off_t>(16 * 1024 * 1024 + sizeof(PCM_HEADER)));
    close(file_descriptor);
    if (truncate_status != 0)
    {
        unlink(path_buffer);
        return (0);
    }
    return (1);
}

static int32_t dumb_expect_load_wav_allocation_failure_with_temp_limit(void)
{
    ft_sound_clip sound_clip_instance;
    char temporary_path[64];
    int32_t result_code;
    int32_t test_success;

#if FT_HAS_VALGRIND_HEADER
    if (RUNNING_ON_VALGRIND != 0)
        return (1);
#endif
    temporary_path[0] = '\0';
    test_success = 0;
    if (dumb_prepare_sparse_wav_file(temporary_path, sizeof(temporary_path)) == 0)
    {
        test_success = 0;
    }
    else if (sound_clip_instance.initialize() != FT_ERR_SUCCESS)
    {
        test_success = 0;
    }
    else
    {
        cma_set_alloc_limit(1);
        result_code = sound_clip_instance.load_wav(temporary_path);
        cma_set_alloc_limit(0);
        (void)sound_clip_instance.destroy();
        if (result_code == FT_ERR_NO_MEMORY)
            test_success = 1;
    }
    if (temporary_path[0] != '\0')
        unlink(temporary_path);
    return (test_success);
}

FT_TEST(test_dumb_sound_clip_load_wav_reports_failure_when_allocation_is_constrained)
{
    FT_ASSERT_EQ(1, dumb_expect_load_wav_allocation_failure_with_temp_limit());
    return (1);
}
