#import <QuartzCore/CAMetalLayer.h>

#include "vremetalview.hpp"

@implementation VREMetalView
+ (Class)layerClass
{
	return ([CAMetalLayer class]);
}
- (CALayer *)makeBackingLayer
{
	return ([CAMetalLayer layer]);
}
@end
