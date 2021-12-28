#ifndef MA_LD_INTERNAL_H
#define MA_LD_INTERNAL_H

#include "multiarcade.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

struct ma_x11;
struct ma_pulse;
struct ma_evdev;

extern struct ma_init_params ma_ld_init_params;
extern uint16_t ma_ld_input;
extern int ma_ld_use_signals;
extern int ma_ld_quit_requested;
extern int64_t ma_ld_next_frame_time;
extern int64_t ma_ld_start_time;
extern int ma_ld_frame_skipc;
extern int ma_ld_framec;
extern const char *ma_ld_file_sandbox;
extern struct ma_x11 *ma_x11;
extern struct ma_pulse *ma_pulse;
extern int ma_ld_audio_locked;
extern struct ma_evdev *ma_evdev;

int64_t ma_ld_now();

/* x11
 ***********************************************************/
 
struct ma_x11 *ma_x11_new(
  const char *title,
  int fbw,int fbh,
  int fullscreen,
  int (*cb_button)(struct ma_x11 *x11,uint16_t btnid,int value),
  int (*cb_close)(struct ma_x11 *x11),
  void *userdata
);

void ma_x11_del(struct ma_x11 *x11);

void *ma_x11_get_userdata(const struct ma_x11 *x11);

// 0=window, >0=fullscreen, <0=query
int ma_x11_set_fullscreen(struct ma_x11 *x11,int state);

int ma_x11_update(struct ma_x11 *x11);

int ma_x11_swap(struct ma_x11 *x11,const void *fb);

void ma_x11_inhibit_screensaver(struct ma_x11 *x11);

/* PulseAudio
 * I've opted for Pulse, for 'linuxdefault'.
 * There's a basically-identical ALSA implementation in 'raspi', which should also work.
 *************************************************************/

struct ma_pulse *ma_pulse_new(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc,struct ma_pulse *pulse),
  void *userdata
);

void ma_pulse_del(struct ma_pulse *pulse);

int ma_pulse_get_rate(const struct ma_pulse *pulse);
int ma_pulse_get_chanc(const struct ma_pulse *pulse);
void *ma_pulse_get_userdata(const struct ma_pulse *pulse);
int ma_pulse_get_status(const struct ma_pulse *pulse); // 0,-1

int ma_pulse_lock(struct ma_pulse *pulse);
int ma_pulse_unlock(struct ma_pulse *pulse);

/* Evdev.
 **********************************************************/
 
struct ma_evdev *ma_evdev_new(
  int (*cb_button)(struct ma_evdev *evdev,uint16_t btnid,int value),
  void *userdata
);

void ma_evdev_del(struct ma_evdev *evdev);

void *ma_evdev_get_userdata(const struct ma_evdev *evdev);

int ma_evdev_update(struct ma_evdev *evdev);

#endif
