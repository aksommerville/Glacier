#include <string.h>
#include <pthread.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include "akmacaudio.h"

/* Globals.
 */

static struct {
  int init;
  int state;
  AudioComponent component;
  AudioComponentInstance instance;
  void (*cb)(int16_t *dst,int dstc);
  pthread_mutex_t cbmtx;
} akmacaudio={0};

/* AudioUnit callback.
 */

static OSStatus akmacaudio_cb(
  void *userdata,
  AudioUnitRenderActionFlags *flags,
  const AudioTimeStamp *time,
  UInt32 bus,
  UInt32 framec,
  AudioBufferList *data
) {
  if (pthread_mutex_lock(&akmacaudio.cbmtx)) return 0;
  akmacaudio.cb(data->mBuffers[0].mData,data->mBuffers[0].mDataByteSize>>1);
  pthread_mutex_unlock(&akmacaudio.cbmtx);
  return 0;
}

/* Init.
 */
 
int akmacaudio_init(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc)
) {
  int err;
  if (akmacaudio.init||!cb) return -1;
  if ((rate<1000)||(rate>1000000)) return -1;
  if ((chanc<1)||(chanc>16)) return -1;
  memset(&akmacaudio,0,sizeof(akmacaudio));

  pthread_mutex_init(&akmacaudio.cbmtx,0);

  akmacaudio.init=1;
  akmacaudio.cb=cb;

  AudioComponentDescription desc={0};
  desc.componentType=kAudioUnitType_Output;
  desc.componentSubType=kAudioUnitSubType_DefaultOutput;

  akmacaudio.component=AudioComponentFindNext(0,&desc);
  if (!akmacaudio.component) {
    return -1;
  }

  if (AudioComponentInstanceNew(akmacaudio.component,&akmacaudio.instance)) {
    return -1;
  }

  if (AudioUnitInitialize(akmacaudio.instance)) {
    return -1;
  }

  AudioStreamBasicDescription fmt={0};
  fmt.mSampleRate=rate;
  fmt.mFormatID=kAudioFormatLinearPCM;
  fmt.mFormatFlags=kAudioFormatFlagIsSignedInteger; // implies little-endian
  fmt.mFramesPerPacket=1;
  fmt.mChannelsPerFrame=chanc;
  fmt.mBitsPerChannel=16;
  fmt.mBytesPerPacket=chanc*2;
  fmt.mBytesPerFrame=chanc*2;

  if (err=AudioUnitSetProperty(akmacaudio.instance,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&fmt,sizeof(fmt))) {
    return -1;
  }

  AURenderCallbackStruct aucb={0};
  aucb.inputProc=akmacaudio_cb;
  aucb.inputProcRefCon=0;

  if (AudioUnitSetProperty(akmacaudio.instance,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&aucb,sizeof(aucb))) {
    return -1;
  }

  if (AudioOutputUnitStart(akmacaudio.instance)) {
    return -1;
  }

  akmacaudio.state=1;

  return 0;
}

/* Quit.
 */

void akmacaudio_quit() {
  if (akmacaudio.state) {
    AudioOutputUnitStop(akmacaudio.instance);
  }
  if (akmacaudio.instance) {
    AudioComponentInstanceDispose(akmacaudio.instance);
  }
  pthread_mutex_destroy(&akmacaudio.cbmtx);
  memset(&akmacaudio,0,sizeof(akmacaudio));
}

/* Callback lock.
 */
 
int akmacaudio_lock() {
  if (pthread_mutex_lock(&akmacaudio.cbmtx)) return -1;
  return 0;
}

int akmacaudio_unlock() {
  if (pthread_mutex_unlock(&akmacaudio.cbmtx)) return -1;
  return 0;
}
