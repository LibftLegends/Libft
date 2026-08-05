#include "bmp.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <fcntl.h>
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

static void bmp_write_u16(uint8_t *data, uint16_t value) noexcept
{
    data[0] = static_cast<uint8_t>(value & 0xFFU);
    data[1] = static_cast<uint8_t>(value >> 8U);
    return ;
}

static void bmp_write_u32(uint8_t *data, uint32_t value) noexcept
{
    data[0] = static_cast<uint8_t>(value & 0xFFU);
    data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    return ;
}

static int32_t bmp_encode(const uint8_t *pixels, ft_size_t width,
    ft_size_t height, uint8_t **file_data_out,
    ft_size_t *file_size_out) noexcept
{
    ft_size_t row_size;
    ft_size_t file_size;
    ft_size_t row_index;
    uint8_t *file_data;

    if (pixels == ft_nullptr || file_data_out == ft_nullptr
        || file_size_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    *file_data_out = ft_nullptr;
    *file_size_out = 0U;
    if (width == 0U || height == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (width > (BMP_HARD_MAX_FILE_SIZE - 54U) / 4U)
        return (FT_ERR_OUT_OF_RANGE);
    row_size = width * 4U;
    if (height > (BMP_HARD_MAX_FILE_SIZE - 54U) / row_size)
        return (FT_ERR_OUT_OF_RANGE);
    file_size = 54U + row_size * height;
    if (file_size > UINT32_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    file_data = new (std::nothrow) uint8_t[file_size];
    if (file_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_memset(file_data, 0, file_size);
    file_data[0] = 'B';
    file_data[1] = 'M';
    bmp_write_u32(file_data + 2U, static_cast<uint32_t>(file_size));
    bmp_write_u32(file_data + 10U, 54U);
    bmp_write_u32(file_data + 14U, 40U);
    bmp_write_u32(file_data + 18U, static_cast<uint32_t>(width));
    bmp_write_u32(file_data + 22U, static_cast<uint32_t>(height));
    bmp_write_u16(file_data + 26U, 1U);
    bmp_write_u16(file_data + 28U, 32U);
    bmp_write_u32(file_data + 30U, 0U);
    bmp_write_u32(file_data + 34U,
        static_cast<uint32_t>(row_size * height));
    row_index = 0U;
    while (row_index < height)
    {
        ft_size_t source_row;
        ft_size_t column_index;
        uint8_t *destination_row;
        const uint8_t *source_row_data;

        source_row = height - 1U - row_index;
        destination_row = file_data + 54U + row_index * row_size;
        source_row_data = pixels + source_row * row_size;
        column_index = 0U;
        while (column_index < width)
        {
            const uint8_t *source_pixel;
            uint8_t *destination_pixel;

            source_pixel = source_row_data + column_index * 4U;
            destination_pixel = destination_row + column_index * 4U;
            destination_pixel[0] = source_pixel[2];
            destination_pixel[1] = source_pixel[1];
            destination_pixel[2] = source_pixel[0];
            destination_pixel[3] = source_pixel[3];
            column_index += 1U;
        }
        row_index += 1U;
    }
    *file_data_out = file_data;
    *file_size_out = file_size;
    return (FT_ERR_SUCCESS);
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
        || maximum_file_size > BMP_HARD_MAX_FILE_SIZE)
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

thread_local int32_t bmp_image::_last_error = FT_ERR_SUCCESS;

int32_t bmp_image::set_error(int32_t error_code) noexcept
{
    bmp_image::_last_error = error_code;
    return (error_code);
}

void bmp_image::reset_fields(void) noexcept
{
    this->_pixels = ft_nullptr;
    this->_width = 0U;
    this->_height = 0U;
    this->_pixel_size = 0U;
    return ;
}

bmp_image::bmp_image() noexcept
    : _pixels(ft_nullptr), _width(0U), _height(0U), _pixel_size(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

bmp_image::~bmp_image() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t bmp_image::initialize() noexcept
{
    int32_t lock_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "bmp_image::initialize", "already initialised");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::initialize(const char *file_path,
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
            "bmp_image::initialize(file)", "already initialised");
    if (maximum_file_size == 0U
        || maximum_file_size > BMP_HARD_MAX_FILE_SIZE)
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

int32_t bmp_image::initialize(const uint8_t *file_data,
    ft_size_t file_size, ft_size_t maximum_file_size) noexcept
{
    uint8_t *pixels;
    ft_size_t width;
    ft_size_t height;
    ft_size_t pixel_size;
    int32_t error_code;
    int32_t lock_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "bmp_image::initialize(data)", "already initialised");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    pixels = ft_nullptr;
    error_code = bmp_decode(file_data, file_size, &pixels, &width, &height,
        &pixel_size, maximum_file_size);
    this->reset_fields();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(error_code));
    }
    this->_pixels = pixels;
    this->_width = width;
    this->_height = height;
    this->_pixel_size = pixel_size;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::initialize_rgb(const uint8_t *rgb_data,
    ft_size_t width, ft_size_t height, ft_bool has_alpha) noexcept
{
    ft_size_t input_pixel_size;
    ft_size_t pixel_size;
    ft_size_t row_index;
    uint8_t *pixels;
    int32_t lock_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "bmp_image::initialize_rgb", "already initialised");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    if (rgb_data == ft_nullptr || width == 0U || height == 0U
        || (has_alpha != FT_FALSE && has_alpha != FT_TRUE))
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(FT_ERR_INVALID_ARGUMENT));
    }
    if (width > (BMP_HARD_MAX_FILE_SIZE - 54U) / 4U
        || height > (BMP_HARD_MAX_FILE_SIZE - 54U) / (width * 4U))
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    input_pixel_size = 3U;
    if (has_alpha == FT_TRUE)
        input_pixel_size = 4U;
    if (width > (BMP_HARD_MAX_FILE_SIZE - 54U) / input_pixel_size
        || height > BMP_HARD_MAX_FILE_SIZE
            / (width * input_pixel_size))
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    pixel_size = width * height * 4U;
    pixels = new (std::nothrow) uint8_t[pixel_size];
    if (pixels == ft_nullptr)
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    row_index = 0U;
    while (row_index < height)
    {
        ft_size_t column_index;

        column_index = 0U;
        while (column_index < width)
        {
            ft_size_t source_index;
            ft_size_t destination_index;

            source_index = (row_index * width + column_index)
                * input_pixel_size;
            destination_index = (row_index * width + column_index) * 4U;
            pixels[destination_index] = rgb_data[source_index];
            pixels[destination_index + 1U] = rgb_data[source_index + 1U];
            pixels[destination_index + 2U] = rgb_data[source_index + 2U];
            if (has_alpha == FT_TRUE)
                pixels[destination_index + 3U] = rgb_data[source_index + 3U];
            else
                pixels[destination_index + 3U] = 255U;
            column_index += 1U;
        }
        row_index += 1U;
    }
    this->reset_fields();
    this->_pixels = pixels;
    this->_width = width;
    this->_height = height;
    this->_pixel_size = pixel_size;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (this->_pixels != ft_nullptr)
        delete[] this->_pixels;
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialization_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (this->set_error(FT_ERR_NO_MEMORY));
    initialization_error = mutex_pointer->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (this->set_error(initialization_error));
    }
    this->_mutex = mutex_pointer;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    if (mutex_pointer == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(destroy_error));
    return (this->set_error(FT_ERR_SUCCESS));
}

ft_bool bmp_image::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::is_thread_safe");
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t bmp_image::move(bmp_image &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "bmp_image::move", "source is uninitialised");
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

int32_t bmp_image::save(const char *file_path) const noexcept
{
    uint8_t *file_data;
    ft_size_t file_size;
    su_file *file;
    ft_size_t write_size;
    int32_t error_code;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (bmp_image::set_error(lock_error));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::save");
    if (file_path == ft_nullptr || file_path[0] == '\0')
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (bmp_image::set_error(FT_ERR_INVALID_ARGUMENT));
    }
    file_data = ft_nullptr;
    file_size = 0U;
    error_code = bmp_encode(this->_pixels, this->_width, this->_height,
        &file_data, &file_size);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (bmp_image::set_error(error_code));
    }
    file = su_fopen(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == ft_nullptr)
    {
        delete[] file_data;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (bmp_image::set_error(FT_ERR_FILE_OPEN_FAILED));
    }
    write_size = su_fwrite(file_data, 1U, file_size, file);
    error_code = su_fclose(file);
    delete[] file_data;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (write_size != file_size || error_code != FT_ERR_SUCCESS)
        return (bmp_image::set_error(FT_ERR_IO));
    return (bmp_image::set_error(FT_ERR_SUCCESS));
}

const uint8_t *bmp_image::data() const noexcept
{
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::data");
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->_pixels);
}

ft_size_t bmp_image::width() const noexcept
{
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (0U);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::width");
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->_width);
}

ft_size_t bmp_image::height() const noexcept
{
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (0U);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::height");
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->_height);
}

ft_size_t bmp_image::pixel_size() const noexcept
{
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (0U);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::pixel_size");
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->_pixel_size);
}

ft_bool bmp_image::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t bmp_image::get_pixel(ft_size_t coordinate_x,
    ft_size_t coordinate_y, uint8_t *red, uint8_t *green,
    uint8_t *blue, uint8_t *alpha) const noexcept
{
    ft_size_t pixel_index;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (bmp_image::set_error(lock_error));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::get_pixel");
    if (red == ft_nullptr || green == ft_nullptr || blue == ft_nullptr
        || alpha == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (bmp_image::set_error(FT_ERR_INVALID_POINTER));
    }
    if (coordinate_x >= this->_width || coordinate_y >= this->_height)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (bmp_image::set_error(FT_ERR_OUT_OF_RANGE));
    }
    pixel_index = (coordinate_y * this->_width + coordinate_x) * 4U;
    *red = this->_pixels[pixel_index];
    *green = this->_pixels[pixel_index + 1U];
    *blue = this->_pixels[pixel_index + 2U];
    *alpha = this->_pixels[pixel_index + 3U];
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (bmp_image::set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::set_pixel(ft_size_t coordinate_x,
    ft_size_t coordinate_y, uint8_t red, uint8_t green,
    uint8_t blue, uint8_t alpha) noexcept
{
    ft_size_t pixel_index;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "bmp_image::set_pixel");
    if (coordinate_x >= this->_width || coordinate_y >= this->_height)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (this->set_error(FT_ERR_OUT_OF_RANGE));
    }
    pixel_index = (coordinate_y * this->_width + coordinate_x) * 4U;
    this->_pixels[pixel_index] = red;
    this->_pixels[pixel_index + 1U] = green;
    this->_pixels[pixel_index + 2U] = blue;
    this->_pixels[pixel_index + 3U] = alpha;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t bmp_image::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "bmp_image::get_error");
    return (bmp_image::_last_error);
}

const char *bmp_image::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "bmp_image::get_error_str");
    return (ft_strerror(bmp_image::_last_error));
}
