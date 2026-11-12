/* The class name the kernel publishes for the Metal plug-in by default is RedwoodMetalDevice (MiniIOMobileFramebuffer.cpp,
 * MetalPluginClassName; boot-arg metal_class= changes it), and the bundle Metal opens is /System/Library/Extensions/
 * RedwoodMetal.bundle (a link to /redwood/lib/RedwoodMetal.bundle in the system image). Metal asks the bundle for the
 * class by that name, and -[NSBundle classNamed:] answers only a class compiled into the bundle itself; this one is,
 * and is HakiosMetalCA's device under the old name, so that the plug-in loads with or without metal_class=.
 * (The superclass is declared here only far enough to subclass it: the runtime links the real one, with its ivars.) */
#import <Foundation/Foundation.h>

@interface HakiosMetalDevice : NSObject
@end

@interface RedwoodMetalDevice : HakiosMetalDevice
@end

@implementation RedwoodMetalDevice
@end
