#include "ps_macwm_internal.h"

@implementation PsWindow

/* Lifecycle.
 */

-(void)dealloc {
  if (self==ps_macwm.window) {
    ps_macwm.window=0;
  }
  [super dealloc];
}

-(id)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag screen:(NSScreen*)screen {

  if (ps_macwm.window) {
    return 0;
  }

  if (!(self=[super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag screen:screen])) {
    return 0;
  }
  
  ps_macwm.window=self;
  self.delegate=self;

  [self makeKeyAndOrderFront:nil];

  return self;
}

/* Preferred public initializer.
 */

+(PsWindow*)newWithWidth:(int)width 
  height:(int)height 
  title:(const char*)title
  fullscreen:(int)fullscreen
{

  NSScreen *screen=[NSScreen mainScreen];
  if (!screen) {
    return 0;
  }
  int screenw=screen.frame.size.width;
  int screenh=screen.frame.size.height;

  if (width>screenw) width=screenw;
  if (width<0) width=0;
  if (height>screenh) height=screenh;
  if (height<0) height=0;

  NSRect bounds=NSMakeRect((screenw>>1)-(width>>1),(screenh>>1)-(height>>1),width,height);

  NSUInteger styleMask=NSTitledWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask|NSResizableWindowMask;
  
  PsWindow *window=[[PsWindow alloc]
    initWithContentRect:bounds
    styleMask:styleMask
    backing:NSBackingStoreNonretained
    defer:0
    screen:0
  ];
  if (!window) return 0;

  window->w=width;
  window->h=height;
  
  window.releasedWhenClosed=0;
  window.acceptsMouseMovedEvents=TRUE;
  window->cursor_visible=1;

  if (title) {
    window.title=[NSString stringWithUTF8String:title];
  }

  if ([window setupOpenGL]<0) {
    [window release];
    return 0;
  }

  if (fullscreen) {
    [window toggleFullScreen:window];
  }
  
  return window;
}

/* Create OpenGL context.
 */

-(int)setupOpenGL {

  NSOpenGLPixelFormatAttribute attrs[]={
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize,8,
    NSOpenGLPFAAlphaSize,0,
    NSOpenGLPFADepthSize,0,
    0
  };
  NSOpenGLPixelFormat *pixelFormat=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  NSRect viewRect=NSMakeRect(0.0,0.0,self.frame.size.width,self.frame.size.height);
  NSOpenGLView *fullScreenView=[[NSOpenGLView alloc] initWithFrame:viewRect pixelFormat:pixelFormat];
  [self setContentView:fullScreenView];
  [pixelFormat release];
  [fullScreenView release];

  context=((NSOpenGLView*)self.contentView).openGLContext;
  if (!context) return -1;
  [context retain];
  [context setView:self.contentView];
  
  [context makeCurrentContext];
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  return 0;
}

/* Frame control.
 */
 
-(int)beginFrame {
  [context makeCurrentContext];
  return 0;
}

-(int)endFrame {
  [context flushBuffer];
  return 0;
}

/* Receive keyboard events.
 */

-(void)keyDown:(NSEvent*)event {

  if (event.modifierFlags&NSCommandKeyMask) {
    return;
  }

  if (!ps_macwm.cb_key) return;

  int state=event.ARepeat?2:1;
  int key=ps_macwm_translate_keysym(event.keyCode);
  if (!key) return;

  ps_macwm_record_key_down(event.keyCode);

  if (ps_macwm.cb_key(key,state)<0) {
    ps_macwm_abort("Failure in key event handler.");
  }
}

-(void)keyUp:(NSEvent*)event {
  ps_macwm_release_key_down(event.keyCode);
  if (!ps_macwm.cb_key) return;
  int key=ps_macwm_translate_keysym(event.keyCode);
  if (!key) return;
  if (ps_macwm.cb_key(key,0)<0) {
    ps_macwm_abort("Failure in key event handler.");
  }
}

-(void)flagsChanged:(NSEvent*)event {
  int nmodifiers=(int)event.modifierFlags;
  if (nmodifiers!=modifiers) {
    int omodifiers=modifiers;
    modifiers=nmodifiers;
    if (ps_macwm.cb_key) {
      int mask=1; for (;mask;mask<<=1) {
    
        if ((nmodifiers&mask)&&!(omodifiers&mask)) {
          int key=ps_macwm_translate_modifier(mask);
          if (key) {
            if (ps_macwm.cb_key(key,1)<0) ps_macwm_abort("Failure in key event handler.");
          }

        } else if (!(nmodifiers&mask)&&(omodifiers&mask)) {
          int key=ps_macwm_translate_modifier(mask);
          if (key) {
            if (ps_macwm.cb_key(key,0)<0) ps_macwm_abort("Failure in key event handler.");
          }
        }
      }
    }
  }
}

/* Mouse events.
 */

static void ps_macwm_event_mouse_motion(NSPoint loc) {

  int wasin=ps_macwm_cursor_within_window();
  ps_macwm.window->mousex=loc.x;
  ps_macwm.window->mousey=ps_macwm.window->h-loc.y;
  int nowin=ps_macwm_cursor_within_window();

  if (!ps_macwm.window->cursor_visible) {
    if (wasin&&!nowin) {
      [NSCursor unhide];
    } else if (!wasin&&nowin) {
      [NSCursor hide];
    }
  }
}

-(void)mouseMoved:(NSEvent*)event { ps_macwm_event_mouse_motion(event.locationInWindow); }
-(void)mouseDragged:(NSEvent*)event { ps_macwm_event_mouse_motion(event.locationInWindow); }
-(void)rightMouseDragged:(NSEvent*)event { ps_macwm_event_mouse_motion(event.locationInWindow); }
-(void)otherMouseDragged:(NSEvent*)event { ps_macwm_event_mouse_motion(event.locationInWindow); }

/* Resize events.
 */

-(void)reportResize:(NSSize)size {

  /* Don't report redundant events (they are sent) */
  int nw=size.width;
  int nh=size.height;
  if ((w==nw)&&(h==nh)) return;
  w=nw;
  h=nh;

  //TODO do we need this?
  //if (ps_video_resized(w,h)<0) {
  //  ps_macwm_abort("Failure in resize handler.");
  //}
}

-(NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize {
  //ps_log(MACIOC,TRACE,"%s",__func__);
  [self reportResize:[self contentRectForFrameRect:(NSRect){.size=frameSize}].size];
  return frameSize;
}

-(NSSize)window:(NSWindow*)window willUseFullScreenContentSize:(NSSize)proposedSize {
  [self reportResize:proposedSize];
  return proposedSize;
}

-(void)windowWillStartLiveResize:(NSNotification*)note {
}

-(void)windowDidEndLiveResize:(NSNotification*)note {
}

-(void)windowDidEnterFullScreen:(NSNotification*)note {
  PsWindow *window=(PsWindow*)note.object;
  if ([window isKindOfClass:PsWindow.class]) {
    window->fullscreen=1;
  }
  if (ps_macwm_drop_all_keys()<0) {
    ps_macwm_abort("Failure in key release handler.");
  }
}

-(void)windowDidExitFullScreen:(NSNotification*)note {
  PsWindow *window=(PsWindow*)note.object;
  if ([window isKindOfClass:PsWindow.class]) {
    window->fullscreen=0;
  }
  if (ps_macwm_drop_all_keys()<0) {
    ps_macwm_abort("Failure in key release handler.");
  }
}

/* NSWindowDelegate hooks.
 */

-(BOOL)window:(NSWindow*)window shouldDragDocumentWithEvent:(NSEvent*)event from:(NSPoint)dragImageLocation withPasteboard:(NSPasteboard*)pasteboard {
  return 1;
}

@end
