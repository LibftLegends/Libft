/**
 * @file vremetalview.hpp
 * @brief A plain NSView whose backing CALayer is a CAMetalLayer, so Vulkan
 * (via MoltenVK) has a Metal layer to build a swapchain-backed drawable
 * from — the macOS/Metal equivalent of WindowLinux handing over a raw
 * X11 Window handle.
 */
#pragma once

#import <Cocoa/Cocoa.h>

@interface VREMetalView : NSView
@end
