#include "../../../Modules/Networking/networking.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    event_loop loop;
    if (event_loop_initialize(&loop) != FT_ERR_SUCCESS)
        return (1);
    int32_t wait_result = FT_ERR_INVALID_STATE;
    uint32_t event_count = 0;
    std::thread waiter([&]()
    {
        event_loop_ready_event event;
        wait_result = event_loop_wait(&loop, &event, 1, &event_count, 1200);
        return ;
    });
    const auto setup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    ft_bool active = FT_FALSE;
    while (!active && std::chrono::steady_clock::now() < setup_deadline)
    {
        if (event_loop_lock(&loop, ft_nullptr) == FT_ERR_SUCCESS)
        {
            active = loop.wait_active;
            event_loop_unlock(&loop, FT_TRUE);
        }
        if (!active)
            std::this_thread::yield();
    }
    const auto start = std::chrono::steady_clock::now();
    const int32_t interrupted = event_loop_interrupt(&loop);
    waiter.join();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    event_loop_destroy(&loop);
    if (!active || interrupted != FT_ERR_SUCCESS || wait_result != FT_ERR_SUCCESS
        || event_count != 0)
        return (1);
    std::printf("%.3f ms from interrupt to wait completion\n", elapsed_ms);
    return (0);
}
