#include "ps_macwm_internal.h"

struct ps_macwm ps_macwm={0};

/* Init.
 */

int ps_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title,
  int (*cb_key)(int usage,int value)
) {
  if (ps_macwm.window) {
    return -1;
  }
  memset(&ps_macwm,0,sizeof(struct ps_macwm));

  ps_macwm.cb_key=cb_key;

  if (!(ps_macwm.window=[PsWindow newWithWidth:w height:h title:title fullscreen:fullscreen])) {
    return -1;
  }

  return 0;
}

/* Quit.
 */

void ps_macwm_quit() {
  [ps_macwm.window release];
  memset(&ps_macwm,0,sizeof(struct ps_macwm));
}

/* Abort.
 */
 
void ps_macwm_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    fprintf(stderr,"%.*s\n",msgc,msg);
  }
  [NSApplication.sharedApplication terminate:nil];
}

/* Test cursor within window, based on last reported position.
 */

int ps_macwm_cursor_within_window() {
  if (!ps_macwm.window) return 0;
  if (ps_macwm.window->mousex<0) return 0;
  if (ps_macwm.window->mousey<0) return 0;
  if (ps_macwm.window->mousex>=ps_macwm.window->w) return 0;
  if (ps_macwm.window->mousey>=ps_macwm.window->h) return 0;
  return 1;
}

/* Show or hide cursor.
 */

int ps_macwm_show_cursor(int show) {
  if (!ps_macwm.window) return -1;
  if (show) {
    if (ps_macwm.window->cursor_visible) return 0;
    if (ps_macwm_cursor_within_window()) {
      [NSCursor unhide];
    }
    ps_macwm.window->cursor_visible=1;
  } else {
    if (!ps_macwm.window->cursor_visible) return 0;
    if (ps_macwm_cursor_within_window()) {
      [NSCursor hide];
    }
    ps_macwm.window->cursor_visible=0;
  }
  return 0;
}

/* Toggle fullscreen.
 */

int ps_macwm_toggle_fullscreen() {

  [ps_macwm.window toggleFullScreen:ps_macwm.window];

  // Take it on faith that the state will change:
  return ps_macwm.window->fullscreen^1;
}

/* OpenGL frame control.
 */

int ps_macwm_flush_video() {
  if (!ps_macwm.window) return -1;
  return [ps_macwm.window endFrame];
}

/* Ridiculous hack to ensure key-up events.
 * Unfortunately, during a fullscreen transition we do not receive keyUp events.
 * If the main input is a keyboard, and the user strikes a key to toggle fullscreen,
 * odds are very strong that they will release that key during the transition.
 * We record every key currently held, and forcibly release them after on a fullscreen transition.
 */
 
int ps_macwm_record_key_down(int key) {
  int p=-1;
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]==key) return 0;
    if (!ps_macwm.keys_down[i]) p=i;
  }
  if (p>=0) {
    ps_macwm.keys_down[p]=key;
  }
  return 0;
}

int ps_macwm_release_key_down(int key) {
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]==key) {
      ps_macwm.keys_down[i]=0;
    }
  }
  return 0;
}

int ps_macwm_drop_all_keys() {
  int i=PS_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (ps_macwm.keys_down[i]) {
      int key=ps_macwm_translate_keysym(ps_macwm.keys_down[i]);
      if (key&&ps_macwm.cb_key) {
        if (ps_macwm.cb_key(key,0)<0) return -1;
      }
      ps_macwm.keys_down[i]=0;
    }
  }
  return 0;
}

/* Trivial accessors.
 */
 
void ps_macwm_get_size(int *w,int *h) {
  *w=ps_macwm.window->w;
  *h=ps_macwm.window->h;
}
