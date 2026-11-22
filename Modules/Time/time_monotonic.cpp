#include "time.hpp"

int64_t   time_monotonic(void)
{
    return (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

