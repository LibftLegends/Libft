#ifndef BMP_HPP
# define BMP_HPP

#include "../Errno/errno.hpp"

#define FT_BMP_HARD_MAX_FILE_SIZE (10U * 1024U * 1024U)
#define FT_BMP_DEFAULT_MAX_FILE_SIZE FT_BMP_HARD_MAX_FILE_SIZE

class ft_bmp_image
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
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        void reset_fields(void) noexcept;

    public:
        ft_bmp_image() noexcept;
        ft_bmp_image(const ft_bmp_image &other) noexcept = delete;
        ft_bmp_image(ft_bmp_image &&other) noexcept = delete;
        ~ft_bmp_image() noexcept;

        ft_bmp_image &operator=(const ft_bmp_image &other) noexcept = delete;
        ft_bmp_image &operator=(ft_bmp_image &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const char *file_path,
            ft_size_t maximum_file_size = FT_BMP_DEFAULT_MAX_FILE_SIZE) noexcept;
        int32_t initialize(const uint8_t *file_data,
            ft_size_t file_size,
            ft_size_t maximum_file_size = FT_BMP_DEFAULT_MAX_FILE_SIZE) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_bmp_image &other) noexcept;

        const uint8_t *data() const noexcept;
        ft_size_t width() const noexcept;
        ft_size_t height() const noexcept;
        ft_size_t pixel_size() const noexcept;
        ft_bool is_initialised() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
