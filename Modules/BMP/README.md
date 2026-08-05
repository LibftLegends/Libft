# BMP

The `BMP` module provides a dependency-free loader for uncompressed BMP images.
Images are decoded into 32-bit RGBA pixels.

`bmp_image` supports 24-bit and 32-bit `BI_RGB` images and validates header,
row, dimension, and allocation bounds. Each load accepts a caller-selected
maximum file size. The hard maximum is `BMP_HARD_MAX_FILE_SIZE` (10 MiB),
and larger requested limits are rejected.

The image object is intended for single-threaded lifecycle use. Callers must
provide external synchronization when sharing an instance across threads or
when running lifecycle operations concurrently with accessors.

- `initialize(file_path, maximum_file_size)` - Load a BMP file.
- `initialize(file_data, file_size, maximum_file_size)` - Decode an in-memory BMP.
- `data()` / `pixel_size()` - Access decoded RGBA bytes.
- `width()` / `height()` - Access decoded dimensions.
- `destroy()` - Release decoded pixels.
- `move(other)` - Explicitly transfer decoded pixels from another initialized image.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` -
  Manage optional recursive-mutex protection for shared lifecycle/accessor use.

The test suite includes embedded 2x1 24-bit bottom-up and 1x2 32-bit top-down
sample BMP images for decoder and orientation coverage.
