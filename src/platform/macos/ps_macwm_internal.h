#ifndef PS_MACWM_INTERNAL_H
#define PS_MACWM_INTERNAL_H

#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#ifndef GL_PROGRAM_POINT_SIZE
  #ifdef GL_PROGRAM_POINT_SIZE_EXT
    #define GL_PROGRAM_POINT_SIZE GL_PROGRAM_POINT_SIZE_EXT
  #elif defined(GL_VERTEX_PROGRAM_POINT_SIZE)
    #define GL_PROGRAM_POINT_SIZE GL_VERTEX_PROGRAM_POINT_SIZE
  #endif
#endif

/* Declaration of window class.
 */

@interface PsWindow : NSWindow <NSWindowDelegate> {
@public
  NSOpenGLContext *context;
  int cursor_visible;
  int w,h;
  int mousex,mousey;
  int modifiers;
  int fullscreen;
}

+(PsWindow*)newWithWidth:(int)width 
  height:(int)height 
  title:(const char*)title
  fullscreen:(int)fullscreen
;

-(int)beginFrame;
-(int)endFrame;

@end

/* Globals.
 */

#define PS_MACWM_KEYS_DOWN_LIMIT 8

extern struct ps_macwm {
  PsWindow *window;
  int keys_down[PS_MACWM_KEYS_DOWN_LIMIT];
  int (*cb_key)(int usage,int value);
} ps_macwm;

/* Miscellaneous.
 */

void ps_macwm_abort(const char *fmt,...);

int ps_macwm_decode_utf8(int *dst,const void *src,int srcc);
int ps_macwm_translate_codepoint(int src);
int ps_macwm_translate_keysym(int src);
int ps_macwm_translate_modifier(int src);

int ps_macwm_record_key_down(int key);
int ps_macwm_release_key_down(int key);
int ps_macwm_drop_all_keys();

int ps_macwm_cursor_within_window();

#endif
