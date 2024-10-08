#include "VRCocoaWindow.h"
#include "core/gui/VRGuiManager.h"
#include "../devices/VRMouse.h"
#include "../devices/VRKeyboard.h"

#include <OpenSG/OSGCocoaWindow.h>
#import <Cocoa/Cocoa.h>

// This prevents warnings that "NSApplication might not
// respond to setAppleMenu" on OS X 10.4
@interface NSApplication(OpenSG)
- (void)setAppleMenu:(NSMenu *)menu;
@end

using namespace OSG;

VRCocoaWindow::VRCocoaWindow() { init(); }
VRCocoaWindow::~VRCocoaWindow() { cleanup(); }

VRCocoaWindowPtr VRCocoaWindow::create() { return VRCocoaWindowPtr( new VRCocoaWindow() ); }
VRCocoaWindowPtr VRCocoaWindow::ptr() { return static_pointer_cast<VRCocoaWindow>(shared_from_this()); }

void VRCocoaWindow::save(XMLElementPtr node) { VRWindow::save(node); }
void VRCocoaWindow::load(XMLElementPtr node) { VRWindow::load(node); }

void VRCocoaWindow::onDisplay() {}
void VRCocoaWindow::onMouse(int b, int s, int x, int y) {}
void VRCocoaWindow::onMotion(int x, int y) {}
void VRCocoaWindow::onKeyboard(int k, int s, int x, int y) {}
void VRCocoaWindow::onKeyboard_special(int k, int s, int x, int y) {}

CocoaWindowUnrecPtr cwin;
VRCocoaWindow* vrCocoaWin = 0;


@interface MyOpenGLView: NSOpenGLView
{
}
- (BOOL) acceptsFirstResponder;

- (void) handleMouseEvent: (NSEvent*) event;

- (void) mouseDown: (NSEvent*) event;
- (void) mouseDragged: (NSEvent*) event;
- (void) mouseUp: (NSEvent*) event;
- (void) mouseMoved:(NSEvent*) event;
- (void) rightMouseDown: (NSEvent*) event;
- (void) rightMouseDragged: (NSEvent*) event;
- (void) rightMouseUp: (NSEvent*) event;
- (void) otherMouseDown: (NSEvent*) event;
- (void) otherMouseDragged: (NSEvent*) event;
- (void) otherMouseUp: (NSEvent*) event;
- (void) scrollWheel: (NSEvent*) event;

// NSMouseMoved

- (void) keyDown: (NSEvent*) event;

- (void) reshape;
- (void) drawRect: (NSRect) bounds;
@end

@implementation MyOpenGLView

- (BOOL) acceptsFirstResponder {
    return YES;
}

- (void) handleMouseEvent: (NSEvent*) event {
    if (!vrCocoaWin) return;
    Real32 a,b,c,d;

    int buttonNumber = [event buttonNumber];
    unsigned int modifierFlags = [event modifierFlags];

    // Traditionally, Apple mice just have one button. It is common practice to simulate
    // the middle and the right button by pressing the option or the control key.
    if (buttonNumber == 0) {
        if (modifierFlags & NSAlternateKeyMask) buttonNumber = 2;
        if (modifierFlags & NSControlKeyMask) buttonNumber = 1;
    }

		if (buttonNumber == 1) buttonNumber = 2;
		else if (buttonNumber == 2) buttonNumber = 1;

		NSWindow* window = [event window];
    NSPoint location = [event locationInWindow];
		float H = [[window contentView] frame].size.height;

		location.x *= 2.0;
		location.y = (H - location.y) * 2.0;
		    //location.y = vrCocoaWin->getSize()[1] - location.y; // invert y

		//cout << " mouse event " << location.x << ", " << location.y << ", " << [event type] << endl;

    switch ([event type]) {
        case NSEventTypeScrollWheel:
            if (event.scrollingDeltaY < 0) buttonNumber = 3;
            else buttonNumber = 4;
						//cout << " mouse scroll " << event.scrollingDeltaY << ", " << buttonNumber << endl;
            if (auto m = vrCocoaWin->getMouse()) m->mouse(buttonNumber, 0, location.x, location.y, 1);
            if (auto m = vrCocoaWin->getMouse()) m->mouse(buttonNumber, 1, location.x, location.y, 1);
            break;

        case NSEventTypeLeftMouseDown:
        case NSEventTypeRightMouseDown:
        case NSEventTypeOtherMouseDown:
            //cout << "mouse down " << buttonNumber << ", x " << location.x << ", y " << location.y << ", H " << H << endl;
            if (auto m = vrCocoaWin->getMouse()) m->mouse(buttonNumber, 0, location.x, location.y, 1);
            break;

        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseUp:
        case NSEventTypeOtherMouseUp:
            //cout << "mouse up " << buttonNumber << endl;
            if (auto m = vrCocoaWin->getMouse()) m->mouse(buttonNumber, 1, location.x, location.y, 1);
            break;

        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
		    case NSEventTypeMouseMoved:
            //cout << "mouse move " << buttonNumber << endl;
            if (auto m = vrCocoaWin->getMouse()) m->motion(location.x, location.y, 1);
            break;

        default:
            break;
    }
}

- (void) mouseDown: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) mouseDragged: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) mouseMoved: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) mouseUp: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) rightMouseDown: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) rightMouseDragged: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) rightMouseUp: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) otherMouseDown: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) otherMouseDragged: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) otherMouseUp: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) scrollWheel: (NSEvent*) event
{
    [self handleMouseEvent: event];
}

- (void) keyDown: (NSEvent*) event
{
    if ([[event characters] length] != 1) return;
    //onKeyDown( [[event characters] characterAtIndex: 0] ); // TODO
}

- (void) reshape
{
    if (!cwin) return;
    [self update];
    NSWindow *window = [self window];
    NSRect frame = [self bounds];
    float scaleFactor = [window backingScaleFactor];
    int W = static_cast<int>(frame.size.width*scaleFactor);
    int H = static_cast<int>(frame.size.height*scaleFactor);
    cwin->resize( W, H );
}

- (void) drawRect: (NSRect) bounds
{
    //redraw();
}

@end

bool doCocoaShutdown = false;

@interface WinDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (BOOL) windowShouldClose: (id) sender;
@end

@implementation WinDelegate
- (BOOL)windowShouldClose:(id)sender {
    cout << " --- cocoa window closed --- " << endl;

    doCocoaShutdown = true;
    vrCocoaWin->cleanup();
    VRGuiManager::trigger("glutCloseWindow",{});
    return YES;
}
@end

@interface MyDelegate : NSObject

{
    NSWindow *window;
    WinDelegate *winDelegate;
    MyOpenGLView *glView;
}

- (void) applicationWillFinishLaunching: (NSNotification*) notification;

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication*) application;

@end

@implementation MyDelegate

- (void) dealloc
{
    [winDelegate release];
    [window release];
    [super dealloc];
}

- (void) applicationWillFinishLaunching: (NSNotification*) notification
{
    int W = 1600;
    int H = 1200;
    winDelegate = [[WinDelegate alloc] init];
    window = [NSWindow alloc];
    NSRect rect = { { 0, 0 }, { double(W), double(H) } };
    [window initWithContentRect: rect styleMask: (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask) backing: NSBackingStoreBuffered defer: YES];
    [window setDelegate:winDelegate];
    [window setTitle: @"PolyVR"];
    [window setReleasedWhenClosed: NO];


    glView = [[MyOpenGLView alloc] autorelease];
    [glView initWithFrame: rect];
    [glView setAutoresizingMask: NSViewMaxXMargin | NSViewWidthSizable | NSViewMaxYMargin | NSViewHeightSizable];
    [[window contentView] addSubview: glView];
		[[glView window] setAcceptsMouseMovedEvents:YES];

    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAWindow,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, NSOpenGLPixelFormatAttribute(16),
        NSOpenGLPixelFormatAttribute(0)
    };
    NSOpenGLPixelFormat *pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes: attrs];
    [glView setPixelFormat: pixFmt];

    // Create OpenSG window
    cwin->setContext ( [glView openGLContext] );
    cwin->init();
    cwin->resize( W, H );

    cwin->activate();

    // do some OpenGL init. Will move into State Chunks later.

    glEnable( GL_DEPTH_TEST );
    glEnable( GL_LIGHTING );
    glEnable( GL_LIGHT0 );

	// Show the window
    [window makeKeyAndOrderFront: nil];
    [window makeFirstResponder:glView];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication*) application
{
    return YES;
}

@end

NSAutoreleasePool *pool = 0;
MyDelegate *delegate = 0;

void VRCocoaWindow::init() {
    vrCocoaWin = this;
    cwin = CocoaWindow::create();

    // A magic method that allows applications to react to events even
    // when they are not organized in a bundle
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    SetFrontProcess(&psn);

    // Create application
    [NSApplication sharedApplication];
    pool = [[NSAutoreleasePool alloc] init];
    delegate =  [[MyDelegate new] autorelease];
    [NSApp setDelegate: delegate];
    [delegate applicationWillFinishLaunching:nil];

    _win = cwin;
}

void VRCocoaWindow::cleanup() {
    cout << " --- cleanup COCOA ---" << endl;
    cwin = 0;
    if (pool) [pool release];
    pool = 0;
}

void VRCocoaWindow::render(bool fromThread) {
  //cout << "VRCocoaWindow::render " << fromThread << endl;
  if (fromThread || doCocoaShutdown) return;

  NSEvent* event = 0;
  do {
      event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
      //NSLog(@"COCOA Event type: %ld", (long)event.type);
      [NSApp sendEvent: event];
      [NSApp updateWindows];
  } while(event != nil);

  VRWindow::render();
}
