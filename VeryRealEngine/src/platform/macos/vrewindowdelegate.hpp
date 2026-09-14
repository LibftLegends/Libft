/**
 * @file vrewindowdelegate.hpp
 * @brief Bridges AppKit window events (close button, resize) back into a
 * plain C++ WindowMacOS instance.
 *
 * Holds a raw, non-owning `owner` pointer: WindowMacOS owns this
 * delegate's lifetime (via CFBridgingRetain/Release in
 * WindowMacOS::initialize()/destroy()), not the other way around.
 */
#pragma once

#import <Cocoa/Cocoa.h>

#include "windowmacos.hpp"

@interface VREWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) vre::WindowMacOS *owner;
@end
