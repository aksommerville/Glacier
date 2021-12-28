/* ps_macwm.h
 * Bare-bones window manager interface for MacOS.
 * Link: -framework Cocoa -framework OpenGL
 */

#ifndef PS_MACWM_H
#define PS_MACWM_H

int ps_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title,
  int (*cb_key)(int usage,int value)
);

void ps_macwm_quit();

int ps_macwm_show_cursor(int show);

// We only provide 'toggle', because it's easy on MacOS, and I think it's all we need.
int ps_macwm_toggle_fullscreen();

int ps_macwm_flush_video();

void ps_macwm_get_size(int *w,int *h);

#endif
