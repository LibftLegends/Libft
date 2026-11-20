#include "ft_gpu_window.hpp"

ft_gpu_window::ft_gpu_window() noexcept
{
    return ;
}

ft_gpu_window::~ft_gpu_window() noexcept
{
    return ;
}

uint32_t ft_gpu_window::primary_press_count() const noexcept
{
    if (this->was_mouse_clicked() != FT_FALSE)
        return (1U);
    return (0U);
}
