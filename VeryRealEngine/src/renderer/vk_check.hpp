/**
 * @file vk_check.hpp
 * @brief The VK_CHECK(expression) macro used throughout the renderer.
 */
#pragma once

#include <cstdio>
#include <cstdlib>
#include <vulkan/vulkan.h>

/**
 * @def VK_CHECK
 * @brief Evaluates a Vulkan call and aborts with a diagnostic message if it
 * did not return VK_SUCCESS.
 *
 * The engine's chosen failure mode for unrecoverable Vulkan errors: a
 * clean, immediate halt with file/line/expression context, rather than
 * continuing with an invalid handle or silently corrupting state.
 * @param expression A call returning VkResult.
 */
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
