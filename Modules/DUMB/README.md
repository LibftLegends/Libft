# DUMB

`DUMB` is a small software-rendering, input, audio, and file utility module for simple demos and tools. Its main classes follow the project lifecycle pattern: construct, `initialize`, use, then `destroy`.

## Controls

- `ft_dumb_control` - Fixed input enum for up, down, left, right, confirm, back, and count.
- `ft_dumb_controls_poll()` - Polls platform input and updates current/pressed state.
- `ft_dumb_control_is_down(ft_dumb_control control)` - Reports whether a control is currently held.
- `ft_dumb_control_was_pressed(ft_dumb_control control)` - Reports whether a control was pressed since the previous poll.
- `ft_dumb_mouse_delta` - Signed X/Y mouse movement delta captured during the last poll.
- `ft_dumb_controls_mouse_delta()` - Returns the most recent mouse movement delta collected by `ft_dumb_controls_poll()`.

## Rendering Types

- `ft_render_window_flags` - Window flags for none, resizable, and fullscreen.
- `ft_render_window_desc` - Window creation settings: width, height, title, and flags.
- `ft_render_framebuffer` - Software framebuffer width, height, and pixel pointer.
- `ft_render_depth_buffer` - Depth buffer width, height, and depth-value pointer.
- `ft_render_shader_input` - Per-pixel shader input: coordinates, dimensions, current color/depth, and user data.
- `ft_render_shader_output` - Per-pixel shader result: color, depth, and whether to write depth.
- `ft_render_fragment_shader` - CPU fragment callback used by `shade`.
- `ft_render_screen_size` - Width/height result for primary screen size.
- `ft_render_get_primary_screen_size()` - Returns the primary display size where supported.

## `ft_render_window`

- Lifecycle: `ft_render_window`, `~ft_render_window`, `initialize`, descriptor initialization, copy/move initialization, `destroy`, and `move`.
- `shutdown()` - Requests window shutdown.
- `poll_events()` - Processes window events and close requests.
- `present()` - Presents the software framebuffer to the platform window.
- `framebuffer()` / `depth_buffer()` - Return mutable framebuffer/depth-buffer references.
- `clear(uint32_t color)` - Clears the framebuffer.
- `put_pixel(...)` - Writes one pixel by coordinate.
- `shade(ft_render_fragment_shader shader, void *user_data)` - Runs a CPU shader callback over the framebuffer.
- `set_fullscreen(ft_bool enabled)` - Toggles fullscreen mode.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional synchronization.
- `should_close()` - Reports whether the window has been asked to close.

## Audio

- `ft_sound_spec` - Audio format and callback description: frequency, channels, samples, callback, and user data.
- `ft_sound_clip` - Lifecycle-managed decoded audio clip.
- `ft_sound_clip::load_wav(...)` - Loads 16-bit PCM RIFF/WAVE samples. A failed load preserves the current clip.
- `ft_sound_clip::get_data()` / `get_size()` / `get_spec()` - Return decoded sample data, byte size, and sound spec.
- `ft_sound_device` - Abstract lifecycle audio output device.
- `ft_sound_device::open(...)` / `close()` - Open or close the platform audio device.
- `ft_sound_device::pause(...)` / `play(...)` / `stop()` - Control audio playback.
  Keep a clip and its sample data alive and unchanged during playback. Call `stop()` before reloading or destroying a clip that may still be playing.
- `ft_create_sound_device()` - Allocates the platform sound device implementation.

Both `ft_sound_clip` and `ft_sound_device` expose lifecycle methods and optional thread-safety toggles.

## File IO

- `ft_read_file(const char *path, char **out_buffer, ft_size_t *out_size)` - Reads a whole file into an allocated buffer.
- `ft_write_file(const char *path, const char *buffer, ft_size_t size)` - Writes a whole buffer to a file.
