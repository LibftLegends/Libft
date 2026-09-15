# BMP

The `BMP` module provides a dependency-free loader for uncompressed BMP images.
Images are decoded into 32-bit RGBA pixels.

`bmp_image` supports 24-bit and 32-bit `BI_RGB` images and validates header,
row, dimension, and allocation bounds. Each load accepts a caller-selected
maximum file size. The hard maximum is `BMP_HARD_MAX_FILE_SIZE` (10 MiB),
and larger requested limits are rejected.

RGB and RGBA byte arrays can be imported with `initialize_rgb(...)`. The
image keeps pixels in RGBA order, and `get_pixel(...)` / `set_pixel(...)`
provide coordinate-based color access. `save(...)` and `encode(...)` write
24-bit RGB or 32-bit RGBA uncompressed BMP data. The 24-bit format discards
alpha and decodes with alpha set to 255.

Images also support in-place fill, horizontal and vertical flips, grayscale,
color inversion, brightness adjustment, nearest-neighbor resizing, and
cropping. `encoded_size(...)` reports the exact buffer size required before
encoding.

The image object is intended for single-threaded lifecycle use. Callers must
provide external synchronization when sharing an instance across threads or
when running lifecycle operations concurrently with accessors.

- `initialize(file_path, maximum_file_size)` - Load a BMP file.
- `initialize(file_data, file_size, maximum_file_size)` - Decode an in-memory BMP.
- `initialize_rgb(rgb_data, width, height, has_alpha)` - Import packed RGB or RGBA bytes.
- `data()` / `pixel_size()` - Access decoded RGBA bytes.
- `width()` / `height()` - Access decoded dimensions.
- `get_pixel(x, y, red, green, blue, alpha)` / `set_pixel(...)` - Read or update one RGBA pixel.
- `encoded_size(bit_depth, size_out)` - Query the encoded size for 24- or 32-bit output.
- `encode(file_data, file_size, written_size, bit_depth)` - Encode into a caller-provided buffer.
- `save(file_path, bit_depth)` - Encode the image as a 24- or 32-bit uncompressed BMP file.
- `fill(red, green, blue, alpha)` - Replace every pixel with one color.
- `flip_horizontal()` / `flip_vertical()` - Mirror the image in place.
- `grayscale()` / `invert_colors()` - Apply RGB color transformations while preserving alpha.
- `adjust_brightness(amount)` - Add a clamped signed amount to each RGB channel.
- `crop(origin_x, origin_y, crop_width, crop_height)` - Keep a validated rectangular region.
- `resize_nearest(new_width, new_height)` - Resize with nearest-neighbor sampling.
- `destroy()` - Release decoded pixels.
- `move(other)` - Explicitly transfer decoded pixels from another initialized image.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` -
  Manage optional recursive-mutex protection for shared lifecycle/accessor use.

The test suite includes embedded 2x1 24-bit bottom-up and 1x2 32-bit top-down
sample BMP images for decoder and orientation coverage.
