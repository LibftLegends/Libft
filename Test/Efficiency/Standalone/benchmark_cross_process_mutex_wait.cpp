#include "../../../Modules/Compatebility/compatebility_cross_process.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <algorithm>
# include <atomic>
# include <chrono>
# include <cstdio>
# include <thread>

int main()
{
    constexpr uint32_t SAMPLE_COUNT = 21;
    using clock = std::chrono::steady_clock;
    double samples[SAMPLE_COUNT];
    uint32_t index = 0;
    while (index < SAMPLE_COUNT)
    {
        HANDLE mutex_handle = CreateMutexA(ft_nullptr, TRUE, ft_nullptr);
        if (mutex_handle == ft_nullptr)
            return (1);
        cross_process_message message{};
        cmp_cross_process_mapping mapping{};
        mapping.mutex_address = &mutex_handle;
        cmp_cross_process_mutex_state state{};
        std::atomic<ft_bool> started(FT_FALSE);
        int32_t result = FT_ERR_INTERNAL;
        clock::time_point acquired;
        std::thread waiter([&]()
        {
            started.store(FT_TRUE, std::memory_order_release);
            result = cmp_cross_process_lock_mutex(message, &mapping, &state);
            acquired = clock::now();
            if (result == FT_ERR_SUCCESS)
                result = cmp_cross_process_unlock_mutex(message, &mapping, &state);
            return ;
        });
        while (started.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        Sleep(10U);
        const auto released = clock::now();
        const auto release_result = ReleaseMutex(mutex_handle);
        waiter.join();
        CloseHandle(mutex_handle);
        if (!release_result || result != FT_ERR_SUCCESS || acquired < released)
            return (1);
        samples[index] = std::chrono::duration<double, std::milli>(acquired - released).count();
        ++index;
    }
    std::sort(samples, samples + SAMPLE_COUNT);
    std::printf("mutex release-to-acquire %.3f ms median (%u samples)\n",
        samples[SAMPLE_COUNT / 2], SAMPLE_COUNT);
    return (0);
}
#else
int main()
{
    return (1);
}
#endif
