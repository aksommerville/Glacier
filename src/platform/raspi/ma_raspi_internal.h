#ifndef MA_RASPI_INTERNAL_H
#define MA_RASPI_INTERNAL_H

#include "multiarcade.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

struct ma_bcm;
struct ma_alsa;
struct ma_evdev;

extern struct ma_init_params ma_raspi_init_params;
extern uint16_t ma_raspi_input;
extern int ma_raspi_use_signals;
extern int ma_raspi_quit_requested;
extern int64_t ma_raspi_next_frame_time;
extern int64_t ma_raspi_start_time;
extern int ma_raspi_frame_skipc;
extern int ma_raspi_framec;
extern const char *ma_raspi_file_sandbox;
extern struct ma_bcm *ma_bcm;
extern struct ma_alsa *ma_alsa;
extern int ma_raspi_audio_locked;
extern struct ma_evdev *ma_evdev;

int64_t ma_raspi_now();

/* Broadcom video.
 ***********************************************************/
 
struct ma_bcm *ma_bcm_new(int fbw,int fbh);
void ma_bcm_del(struct ma_bcm *bcm);
void ma_bcm_swap(struct ma_bcm *bcm,const void *fb);

/* ALSA.
 *************************************************************/

struct ma_alsa *ma_alsa_new(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc,struct ma_alsa *alsa),
  void *userdata
);

void ma_alsa_del(struct ma_alsa *alsa);

int ma_alsa_get_rate(const struct ma_alsa *alsa);
int ma_alsa_get_chanc(const struct ma_alsa *alsa);
void *ma_alsa_get_userdata(const struct ma_alsa *alsa);
int ma_alsa_get_status(const struct ma_alsa *alsa); // 0,-1

int ma_alsa_lock(struct ma_alsa *alsa);
int ma_alsa_unlock(struct ma_alsa *alsa);

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
