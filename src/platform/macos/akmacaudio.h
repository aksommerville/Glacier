/* akmacaudio.h
 * Simple API to produce an audio output stream for MacOS.
 * Link: -framework AudioUnit
 */

#ifndef AKMACAUDIO_H
#define AKMACAUDIO_H

#include <stdint.h>

int akmacaudio_init(int rate,int chanc,void (*cb)(int16_t *dst,int dstc));
void akmacaudio_quit();

int akmacaudio_lock();
int akmacaudio_unlock();

#endif
