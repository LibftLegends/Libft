#include "bmp.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <cstdio>
#include <new>

static uint16_t bmp_read_u16(const uint8_t *data) noexcept
{
    return (static_cast<uint16_t>(data[0])
        | static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8U));
}

static uint32_t bmp_read_u32(const uint8_t *data) noexcept
{
    return (static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8U)
        | (static_cast<uint32_t>(data[2]) << 16U)
        | (static_cast<uint32_t>(data[3]) << 24U));
}

static int32_t bmp_read_i32(const uint8_t *data) noexcept
{
    return (static_cast<int32_t>(bmp_read_u32(data)));
}

static int32_t bmp_decode(const uint8_t *file_data, ft_size_t file_size,
    uint8_t **pixels_out, ft_size_t *width_out, ft_size_t *height_out,
    ft_size_t *pixel_size_out, ft_size_t maximum_file_size) noexcept
{
    uint32_t dib_header_size;
    uint32_t pixel_offset;
    int32_t width_signed;
    int32_t height_signed;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    ft_size_t width;
    ft_size_t height;
    ft_size_t bytes_per_pixel;
    ft_size_t row_size;
    ft_size_t pixel_size;
    ft_size_t row_index;
    uint8_t *pixels;

    if (file_data == ft_nullptr || pixels_out == ft_nullptr
        || width_out == ft_nullptr || height_out == ft_nullptr
        || pixel_size_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    *pixels_out = ft_nullptr;
    *width_out = 0U;
    *height_out = 0U;
    *pixel_size_out = 0U;
    if (maximum_file_size == 0U
        || maximum_file_size > FT_BMP_HARD_MAX_FILE_SIZE)
        return (FT_ERR_OUT_OF_RANGE);
    if (file_size > maximum_file_size || file_size < 54U)
        return (FT_ERR_OUT_OF_RANGE);
    if (file_data[0] != 'B' || file_data[1] != 'M')
        return (FT_ERR_INVALID_ARGUMENT);
    dib_header_size = bmp_read_u32(file_data + 14U);
    if (dib_header_size < 40U || dib_header_size > file_size - 14U)
        return (FT_ERR_INVALID_ARGUMENT);
    pixel_offset = bmp_read_u32(file_data + 10U);
    if (pixel_offset < 14U + dib_header_size || pixel_offset >= file_size)
        return (FT_ERR_INVALID_ARGUMENT);
    width_signed = bmp_read_i32(file_data + 18U);
    height_signed = bmp_read_i32(file_data + 22U);
    planes = bmp_read_u16(file_data + 26U);
    bits_per_pixel = bmp_read_u16(file_data + 28U);
    compression = bmp_read_u32(file_data + 30U);
    if (width_signed <= 0 || height_signed == 0 || height_signed == INT32_MIN
        || planes != 1U || (bits_per_pixel != 24U && bits_per_pixel != 32U)
        || compression != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    width = static_cast<ft_size_t>(width_signed);
    if (height_signed < 0)
        height = static_cast<ft_size_t>(-(height_signed + 1)) + 1U;
    else
        height = static_cast<ft_size_t>(height_signed);
    bytes_per_pixel = static_cast<ft_size_t>(bits_per_pixel / 8U);
    if (width > (maximum_file_size - 3U) / bytes_per_pixel)
        return (FT_ERR_OUT_OF_RANGE);
    row_size = ((width * bytes_per_pixel + 3U) / 4U) * 4U;
    if (height > maximum_file_size / row_size)
        return (FT_ERR_OUT_OF_RANGE);
    if (width > maximum_file_size / 4U)
        return (FT_ERR_OUT_OF_RANGE);
    if (height > maximum_file_size / (width * 4U))
        return (FT_ERR_OUT_OF_RANGE);
    pixel_size = width * height * 4U;
    if (pixel_offset > file_size || row_size > file_size - pixel_offset
        || height > (file_size - pixel_offset) / row_size)
        return (FT_ERR_INVALID_ARGUMENT);
    pixels = new (std::nothrow) uint8_t[pixel_size];
    if (pixels == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    row_index = 0U;
    while (row_index < height)
    {
        ft_size_t source_row;
        ft_size_t column_index;
        const uint8_t *source;

        if (height_signed < 0)
            source_row = row_index;
        else
            source_row = height - 1U - row_index;
        source = file_data + pixel_offset + source_row * row_size;
        column_index = 0U;
        while (column_index < width)
        {
            const uint8_t *source_pixel;
            uint8_t *destination_pixel;

            source_pixel = source + column_index * bytes_per_pixel;
            destination_pixel = pixels + (row_index * width
                + column_index) * 4U;
            destination_pixel[0] = source_pixel[2];
            destination_pixel[1] = source_pixel[1];
            destination_pixel[2] = source_pixel[0];
            if (bytes_per_pixel == 4U)
                destination_pixel[3] = source_pixel[3];
            else
                destination_pixel[3] = 255U;
            column_index += 1U;
        }
        row_index += 1U;
    }
    *pixels_out = pixels;
    *width_out = width;
    *height_out = height;
    *pixel_size_out = pixel_size;
    return (FT_ERR_SUCCESS);
}

thread_local int32_t ft_bmp_image::_last_error = FT_ERR_SUCCESS;

int32_t ft_bmp_image::set_error(int32_t error_code) noexcept
{
    ft_bmp_image::_last_error = error_code;
    return (error_code);
}

void ft_bmp_image::reset_fields(void) noexcept
{
    this->_pixels = ft_nullptr;
    this->_width = 0U;
    this->_height = 0U;
    this->_pixel_size = 0U;
    return ;
}

ft_bmp_image::ft_bmp_image() noexcept
    : _pixels(ft_nullptr), _width(0U), _height(0U), _pixel_size(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bmp_image::~ft_bmp_image() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_bmp_image::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_bmp_image::initialize", "already initialised");
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_bmp_image::initialize(const char *file_path,
    ft_size_t maximum_file_size) noexcept
{
    su_file *file;
    int64_t native_file_size;
    ft_size_t file_size;
    uint8_t *file_data;
    ft_size_t read_size;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_bmp_image::initialize(file)", "already initialised");
    if (maximum_file_size == 0U
        || maximum_file_size > FT_BMP_HARD_MAX_FILE_SIZE)
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    if (file_path == ft_nullptr || file_path[0] == '\0')
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    file = su_fopen(file_path);
    if (file == ft_nullptr)
        return (this->set_error(FT_ERR_FILE_OPEN_FAILED));
    if (su_fseek(file, 0, SEEK_END) != FT_ERR_SUCCESS)
    {
        (void)su_fclose(file);
        return (this->set_error(FT_ERR_IO));
    }
    native_file_size = su_ftell(file);
    if (native_file_size < 0)
    {
        (void)su_fclose(file);
        return (this->set_error(FT_ERR_IO));
    }
    if (native_file_size > static_cast<int64_t>(maximum_file_size))
    {
        (void)su_fclose(file);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    file_size = static_cast<ft_size_t>(native_file_size);
    file_data = new (std::nothrow) uint8_t[file_size];
    if (file_data == ft_nullptr && file_size != 0U)
    {
        (void)su_fclose(file);
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    (void)su_fseek(file, 0, SEEK_SET);
    read_size = su_fread(file_data, 1U, file_size, file);
    (void)su_fclose(file);
    if (read_size != file_size)
    {
        delete[] file_data;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(FT_ERR_IO));
    }
    error_code = this->initialize(file_data, file_size, maximum_file_size);
    delete[] file_data;
    return (error_code);
}

int32_t ft_bmp_image::initialize(const uint8_t *file_data,
    ft_size_t file_size, ft_size_t maximum_file_size) noexcept
{
    uint8_t *pixels;
    ft_size_t width;
    ft_size_t height;
    ft_size_t pixel_size;
    int32_t error_code;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_bmp_image::initialize(data)", "already initialised");
    pixels = ft_nullptr;
    error_code = bmp_decode(file_data, file_size, &pixels, &width, &height,
        &pixel_size, maximum_file_size);
    this->reset_fields();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(error_code));
    }
    this->_pixels = pixels;
    this->_width = width;
    this->_height = height;
    this->_pixel_size = pixel_size;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_bmp_image::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    if (this->_pixels != ft_nullptr)
        delete[] this->_pixels;
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t ft_bmp_image::move(ft_bmp_image &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_bmp_image::move", "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    this->reset_fields();
    this->_initialised_state = other._initialised_state;
    this->_pixels = other._pixels;
    this->_width = other._width;
    this->_height = other._height;
    this->_pixel_size = other._pixel_size;
    other.reset_fields();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(FT_ERR_SUCCESS));
}

const uint8_t *ft_bmp_image::data() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_bmp_image::data");
    return (this->_pixels);
}

ft_size_t ft_bmp_image::width() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_bmp_image::width");
    return (this->_width);
}

ft_size_t ft_bmp_image::height() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_bmp_image::height");
    return (this->_height);
}

ft_size_t ft_bmp_image::pixel_size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_bmp_image::pixel_size");
    return (this->_pixel_size);
}

ft_bool ft_bmp_image::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_bmp_image::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "ft_bmp_image::get_error");
    return (ft_bmp_image::_last_error);
}

const char *ft_bmp_image::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "ft_bmp_image::get_error_str");
    return (ft_strerror(ft_bmp_image::_last_error));
}
