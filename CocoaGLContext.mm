#import <QtPlatformHeaders/QCocoaNativeContext>

#import "CocoaGLContext.h"
#include <iostream>

void * GetCocoaGLContext(QVariant x)
{
    QCocoaNativeContext p = (x.value<QCocoaNativeContext>());
    NSOpenGLContext * gl = p.context();
    NSOpenGLPixelFormat *pf = [gl pixelFormat];
    GLint value;
    [pf getValues:&value forAttribute:NSOpenGLPFADepthSize forVirtualScreen:0];
    NSLog(@"depth size: %d", value);
    [pf getValues:&value forAttribute:NSOpenGLPFAStencilSize forVirtualScreen:0];
    NSLog(@"stencil size: %d", value);
    [pf getValues:&value forAttribute:NSOpenGLPFAOpenGLProfile forVirtualScreen:0];
    NSLog(@"profile 3.2: %d == %d", value, NSOpenGLProfileVersion3_2Core);
    NSLog(@"profile 4.1: %d == %d", value, NSOpenGLProfileVersion4_1Core);
    NSLog(@"profile legacy: %d == %d", value, NSOpenGLProfileVersionLegacy);
    [pf getValues:&value forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:0];
    NSLog(@"doubel buffer: %d", value);
    [pf getValues:&value forAttribute:NSOpenGLPFAAccelerated forVirtualScreen:0];
    NSLog(@"accelerated : %d", value);
    [pf getValues:&value forAttribute:NSOpenGLPFANoRecovery forVirtualScreen:0];
    NSLog(@"recovery: %d", value);
    return gl;
    //return [gl CGLContextObj];
}
