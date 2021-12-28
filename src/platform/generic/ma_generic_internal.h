#ifndef MA_GENERIC_INTERNAL_H
#define MA_GENERIC_INTERNAL_H

#include "multiarcade.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern struct ma_init_params ma_generic_init_params;
extern uint16_t ma_generic_input;
extern int ma_generic_use_signals;
extern int ma_generic_use_stdin;
extern int ma_generic_use_stdout;
extern int ma_generic_quit_requested;
extern int64_t ma_generic_next_frame_time;
extern int64_t ma_generic_start_time;
extern int ma_generic_frame_skipc;
extern int ma_generic_framec;
extern const char *ma_generic_file_sandbox;
extern int ma_generic_keyhold[6]; // (left,right,up,down,a,b), frames remaining
extern int ma_generic_keyhold_time;
extern uint16_t ma_generic_input_next;

int64_t ma_generic_now();

#endif
