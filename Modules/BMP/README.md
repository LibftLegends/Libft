# BMP

The `BMP` module provides a dependency-free loader for uncompressed BMP images.
Images are decoded into 32-bit RGBA pixels.

`ft_bmp_image` supports 24-bit and 32-bit `BI_RGB` images and validates header,
row, dimension, and allocation bounds. Each load accepts a caller-selected
maximum file size. The hard maximum is `FT_BMP_HARD_MAX_FILE_SIZE` (10 MiB),
and larger requested limits are rejected.

- `initialize(file_path, maximum_file_size)` - Load a BMP file.
- `initialize(file_data, file_size, maximum_file_size)` - Decode an in-memory BMP.
- `data()` / `pixel_size()` - Access decoded RGBA bytes.
- `width()` / `height()` - Access decoded dimensions.
- `destroy()` - Release decoded pixels.
- `move(other)` - Explicitly transfer decoded pixels from another initialized image.
