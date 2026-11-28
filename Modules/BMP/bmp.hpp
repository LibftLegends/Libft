#ifndef BMP_HPP
# define BMP_HPP

#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"

#define BMP_HARD_MAX_FILE_SIZE (10U * 1024U * 1024U)
#define BMP_DEFAULT_MAX_FILE_SIZE BMP_HARD_MAX_FILE_SIZE
#define BMP_BIT_DEPTH_RGB24 24U
#define BMP_BIT_DEPTH_RGBA32 32U

class bmp_image
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t *_pixels;
        ft_size_t _width;
        ft_size_t _height;
        ft_size_t _pixel_size;
        uint8_t _initialised_state;
        pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        void reset_fields(void) noexcept;

    public:
        bmp_image() noexcept;
        bmp_image(const bmp_image &other) noexcept = delete;
        bmp_image(bmp_image &&other) noexcept = delete;
        ~bmp_image() noexcept;

        bmp_image &operator=(const bmp_image &other) noexcept = delete;
        bmp_image &operator=(bmp_image &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const char *file_path,
            ft_size_t maximum_file_size = BMP_DEFAULT_MAX_FILE_SIZE) noexcept;
        int32_t initialize(const uint8_t *file_data,
            ft_size_t file_size,
            ft_size_t maximum_file_size = BMP_DEFAULT_MAX_FILE_SIZE) noexcept;
        int32_t initialize_rgb(const uint8_t *rgb_data,
            ft_size_t width, ft_size_t height,
            ft_bool has_alpha = FT_FALSE) noexcept;
        int32_t destroy() noexcept;
        int32_t move(bmp_image &other) noexcept;
        int32_t encoded_size(uint16_t bit_depth,
            ft_size_t *size_out) const noexcept;
        int32_t encode(uint8_t *file_data, ft_size_t file_size,
            ft_size_t *written_size, uint16_t bit_depth) const noexcept;
        int32_t save(const char *file_path,
            uint16_t bit_depth = BMP_BIT_DEPTH_RGBA32) const noexcept;
        int32_t fill(uint8_t red, uint8_t green, uint8_t blue,
            uint8_t alpha = 255U) noexcept;
        int32_t flip_horizontal() noexcept;
        int32_t flip_vertical() noexcept;
        int32_t grayscale() noexcept;
        int32_t invert_colors() noexcept;
        int32_t adjust_brightness(int32_t amount) noexcept;
        int32_t crop(ft_size_t origin_x, ft_size_t origin_y,
            ft_size_t crop_width, ft_size_t crop_height) noexcept;
        int32_t resize_nearest(ft_size_t new_width,
            ft_size_t new_height) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        const uint8_t *data() const noexcept;
        ft_size_t width() const noexcept;
        ft_size_t height() const noexcept;
        ft_size_t pixel_size() const noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t get_pixel(ft_size_t coordinate_x, ft_size_t coordinate_y,
            uint8_t *red, uint8_t *green, uint8_t *blue,
            uint8_t *alpha) const noexcept;
        int32_t set_pixel(ft_size_t coordinate_x, ft_size_t coordinate_y,
            uint8_t red, uint8_t green, uint8_t blue,
            uint8_t alpha = 255U) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
