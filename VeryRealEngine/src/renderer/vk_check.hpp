#pragma once

#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

#define VK_CHECK(expression)                                                   \
    do                                                                         \
    {                                                                          \
        VkResult vk_check_result = (expression);                              \
        if (vk_check_result != VK_SUCCESS)                                    \
        {                                                                      \
            std::fprintf(stderr, "Vulkan error %d at %s:%d in %s\n",          \
                static_cast<int>(vk_check_result), __FILE__, __LINE__,        \
                #expression);                                                  \
            std::abort();                                                      \
        }                                                                       \
    } while (0)
