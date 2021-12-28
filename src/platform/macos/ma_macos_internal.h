#ifndef MA_MACOS_INTERNAL_H
#define MA_MACOS_INTERNAL_H

//TODO Remove "ps_" etc prefixes, we are not Plunder Squad anymore.

#include "multiarcade.h"
#include "ps_machid.h"
#include "akmacaudio.h"
#include "ps_macwm.h"
#include "ps_macioc.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct ma_init_params ma_macos_init_params;
extern uint16_t ma_macos_input_state;

/* Triggers (cb) with candidate paths until you return nonzero or we run out of ideas.
 * (clientpath) comes from the app, eg "/Sitter/map/00000".
 * cb(path) is local, eg "/Users/flubberdub/Library/Applications/com.aksommerville.lilsitter/map/00000".
 */
int ma_macos_mangle_path(
  const char *clientpath,
  int (*cb)(const char *path,void *userdata),
  void *userdata
);

#endif
