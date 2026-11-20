#if defined(__APPLE__)

#include "gpgr_window_macos.hpp"
#include "../Errno/errno_internal.hpp"

@interface ft_gpu_window_delegate : NSObject<NSWindowDelegate>
{
    ft_bool *_close_flag;
}
- (instancetype)initWithFlag:(ft_bool *)flag;
@end

@implementation ft_gpu_window_delegate
- (instancetype)initWithFlag:(ft_bool *)flag
{
    self = [super init];
    if (self != nil)
        self->_close_flag = flag;
    return (self);
}

- (BOOL)windowShouldClose:(id)sender
{
    (void)sender;
    if (self->_close_flag != nullptr)
        *self->_close_flag = FT_TRUE;
    return (NO);
}
@end

@interface ft_gpu_open_gl_view : NSOpenGLView
@end

@implementation ft_gpu_open_gl_view
- (BOOL)acceptsFirstResponder { return (YES); }
- (void)keyDown:(NSEvent *)event { (void)event; }
- (void)keyUp:(NSEvent *)event { (void)event; }
@end

struct ft_gpu_window_macos_state
{
    NSWindow         *window;
    ft_gpu_open_gl_view    *gl_view;
    NSOpenGLContext  *gl_context;
    ft_gpu_window_delegate *delegate;
    ft_bool           close_flag;
    int32_t           width;
    int32_t           height;
    int32_t           mouse_x;
    int32_t           mouse_y;
    uint32_t          primary_presses;
    ft_bool           settings_key_pressed;
    ft_bool           cursor_visible;
};

class ft_gpu_window_macos : public ft_gpu_window
{
    ft_gpu_window_macos_state *_state;
    uint8_t _initialised_state;

    public:
        ft_gpu_window_macos() noexcept
            : _state(nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED) {}
        ft_gpu_window_macos(const ft_gpu_window_macos &other) = delete;
        ft_gpu_window_macos(ft_gpu_window_macos &&other) = delete;
        ft_gpu_window_macos &operator=(const ft_gpu_window_macos &other)
            = delete;
        ft_gpu_window_macos &operator=(ft_gpu_window_macos &&other) = delete;
        ~ft_gpu_window_macos() override { (void)destroy(); }

        ft_bool initialize(const char *title, int32_t width, int32_t height,
            ft_bool fullscreen) noexcept override;
        int32_t destroy() noexcept override;
        void poll_events() noexcept override;
        void swap_buffers() noexcept override;
        ft_bool should_close() const noexcept override;
        int32_t get_width() const noexcept override;
        int32_t get_height() const noexcept override;
        int32_t get_mouse_x() const noexcept override;
        int32_t get_mouse_y() const noexcept override;
        uint32_t primary_press_count() const noexcept override;
        ft_bool was_mouse_clicked() const noexcept override;
        void set_cursor_visible(ft_bool visible) noexcept override;
        ft_bool was_settings_key_pressed() const noexcept override;
        int32_t move(ft_gpu_window &other) noexcept override;
};

ft_bool ft_gpu_window_macos::initialize(const char *title, int32_t width,
    int32_t height, ft_bool fullscreen) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_gpu_window_macos::initialize",
            "called while object is already initialised");
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, static_cast<NSOpenGLPixelFormatAttribute>(
            NSOpenGLProfileVersion3_2Core),
        NSOpenGLPFAColorSize,   static_cast<NSOpenGLPixelFormatAttribute>(24),
        NSOpenGLPFADepthSize,   static_cast<NSOpenGLPixelFormatAttribute>(24),
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        static_cast<NSOpenGLPixelFormatAttribute>(0)
    };

    this->_state = new (std::nothrow) ft_gpu_window_macos_state();
    if (this->_state == nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }
    this->_state->close_flag           = FT_FALSE;
    this->_state->mouse_x              = 0;
    this->_state->mouse_y              = 0;
    this->_state->primary_presses     = 0U;
    this->_state->settings_key_pressed = FT_FALSE;
    this->_state->cursor_visible       = FT_TRUE;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc]
        initWithAttributes:attrs];
    if (fmt == nil)
    {
        delete this->_state;
        this->_state = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    NSRect content_rect;
    NSUInteger style;
    if (fullscreen == FT_TRUE)
    {
        content_rect = [[NSScreen mainScreen] frame];
        style = NSWindowStyleMaskBorderless;
        this->_state->width  = static_cast<int32_t>(content_rect.size.width);
        this->_state->height = static_cast<int32_t>(content_rect.size.height);
    }
    else
    {
        content_rect = NSMakeRect(0, 0,
            static_cast<CGFloat>(width), static_cast<CGFloat>(height));
        style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable;
        this->_state->width  = width;
        this->_state->height = height;
    }

    this->_state->gl_view = [[ft_gpu_open_gl_view alloc]
        initWithFrame:content_rect pixelFormat:fmt];
    if (this->_state->gl_view == nil)
    {
        delete this->_state;
        this->_state = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }
    [this->_state->gl_view setWantsBestResolutionOpenGLSurface:NO];

    this->_state->window = [[NSWindow alloc]
        initWithContentRect:content_rect
        styleMask:style
        backing:NSBackingStoreBuffered
        defer:NO];
    if (this->_state->window == nil)
    {
        delete this->_state;
        this->_state = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_FALSE);
    }

    this->_state->delegate = [[ft_gpu_window_delegate alloc]
        initWithFlag:&this->_state->close_flag];
    [this->_state->window setDelegate:this->_state->delegate];
    [this->_state->window setTitle:[NSString stringWithUTF8String:title]];
    [this->_state->window setContentView:this->_state->gl_view];
    [this->_state->window makeKeyAndOrderFront:nil];
    [this->_state->window makeFirstResponder:this->_state->gl_view];
    [NSApp activateIgnoringOtherApps:YES];

    if (fullscreen == FT_TRUE)
        [this->_state->window setLevel:NSMainMenuWindowLevel + 1];

    this->_state->gl_context = [this->_state->gl_view openGLContext];
    [this->_state->gl_context makeCurrentContext];

    GLint swap = 1;
    [this->_state->gl_context setValues:&swap
        forParameter:NSOpenGLContextParameterSwapInterval];

    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_TRUE);
}

int32_t ft_gpu_window_macos::destroy() noexcept
{
    if (this->_state == nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    [NSOpenGLContext clearCurrentContext];
    if (this->_state->window != nil)
        [this->_state->window close];
    delete this->_state;
    this->_state = nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_gpu_window_macos::move(ft_gpu_window &other) noexcept
{
    ft_gpu_window_macos &other_window =
        static_cast<ft_gpu_window_macos &>(other);

    if (this == &other_window)
        return (FT_ERR_SUCCESS);
    if (other_window._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other_window._initialised_state,
            "ft_gpu_window_macos::move", "source object is uninitialised");
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other_window._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_state = nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_state = other_window._state;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other_window._state = nullptr;
    other_window._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

void ft_gpu_window_macos::poll_events() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::poll_events");
    this->_state->primary_presses     = 0U;
    this->_state->settings_key_pressed = FT_FALSE;
    @autoreleasepool
    {
        NSEvent *event;
        while ((event = [NSApp
                nextEventMatchingMask:NSEventMaskAny
                untilDate:nil
                inMode:NSDefaultRunLoopMode
                dequeue:YES]) != nil)
        {
            NSEventType t = [event type];
            if (t == NSEventTypeKeyDown)
            {
                if ([event keyCode] == 46U)
                    this->_state->settings_key_pressed = FT_TRUE;
                continue;
            }
            if (t == NSEventTypeKeyUp || t == NSEventTypeFlagsChanged)
                continue;
            if (t == NSEventTypeLeftMouseDown
                && this->_state->primary_presses < UINT32_MAX)
                this->_state->primary_presses += 1U;
            if (t == NSEventTypeMouseMoved || t == NSEventTypeLeftMouseDragged
                || t == NSEventTypeRightMouseDragged
                || t == NSEventTypeLeftMouseDown || t == NSEventTypeLeftMouseUp)
            {
                NSPoint loc = [event locationInWindow];
                if (this->_state->window != nil)
                {
                    NSRect fr = [[this->_state->window contentView] frame];
                    this->_state->mouse_x = static_cast<int32_t>(loc.x);
                    this->_state->mouse_y = static_cast<int32_t>(fr.size.height - loc.y);
                }
            }
            [NSApp sendEvent:event];
        }
    }
    if (this->_state->gl_context != nil)
        [this->_state->gl_context makeCurrentContext];
}

void ft_gpu_window_macos::swap_buffers() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::swap_buffers");
    if (this->_state != nullptr && this->_state->gl_context != nil)
        [this->_state->gl_context flushBuffer];
}

ft_bool ft_gpu_window_macos::should_close() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::should_close");
    return (this->_state != nullptr && this->_state->close_flag);
}

int32_t ft_gpu_window_macos::get_width() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::get_width");
    if (this->_state != nullptr)
        return (this->_state->width);
    return (0);
}

int32_t ft_gpu_window_macos::get_height() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::get_height");
    if (this->_state != nullptr)
        return (this->_state->height);
    return (0);
}

int32_t ft_gpu_window_macos::get_mouse_x() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::get_mouse_x");
    if (this->_state != nullptr)
        return (this->_state->mouse_x);
    return (0);
}

int32_t ft_gpu_window_macos::get_mouse_y() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::get_mouse_y");
    if (this->_state != nullptr)
        return (this->_state->mouse_y);
    return (0);
}

uint32_t ft_gpu_window_macos::primary_press_count() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::primary_press_count");
    if (this->_state == nullptr)
        return (0U);
    return (this->_state->primary_presses);
}

ft_bool ft_gpu_window_macos::was_mouse_clicked() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::was_mouse_clicked");
    return (this->_state != nullptr && this->_state->primary_presses != 0U);
}

void ft_gpu_window_macos::set_cursor_visible(ft_bool visible) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::set_cursor_visible");
    if (visible == FT_TRUE && this->_state->cursor_visible == FT_FALSE)
        [NSCursor unhide];
    else if (visible == FT_FALSE && this->_state->cursor_visible == FT_TRUE)
        [NSCursor hide];
    this->_state->cursor_visible = visible;
    return ;
}

ft_bool ft_gpu_window_macos::was_settings_key_pressed() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_gpu_window_macos::was_settings_key_pressed");
    return (this->_state != nullptr && this->_state->settings_key_pressed);
}

ft_gpu_window *ft_gpu_window::create() noexcept
{
    return (new (std::nothrow) ft_gpu_window_macos());
}

#endif
