#include "bba.h"
#include "bb_midi.h"
#include <string.h>
#include <stdio.h>

/* Initialize.
 */
 
void bba_init(struct bba *bba,uint16_t rate) {
  bba->rate=rate;
  if (!(bba->framespertick=rate/BBA_TICK_RATE)) bba->framespertick=1;
  struct bba_channel *channel=bba->channelv;
  uint8_t i=BBA_CHANNEL_COUNT;
  for (;i-->0;channel++) {
    channel->reload=1;
  }
}

/* Apply external configurations -- config and song.
 */
 
void bba_configure(struct bba *bba,const void *src,uint16_t srcc) {
  if (src&&(srcc>0)) {
    bba->config=src;
    bba->configc=srcc;
  } else {
    bba->config=0;
    bba->configc=0;
  }
  struct bba_channel *channel=bba->channelv;
  uint8_t i=BBA_CHANNEL_COUNT;
  for (;i-->0;channel++) {
    channel->reload=1;
  }
}

void bba_play_song(struct bba *bba,const void *src,uint16_t srcc) {
  if (src==bba->song) return;
  if (src&&(srcc>0)) {
    bba->song=src;
    bba->songc=srcc;
  } else {
    bba->song=0;
    bba->songc=0;
  }
  bba->songp=0;
  bba->songdelay=0;
  bba->songrepeat=1;
  bba_release_voices(bba);
}

/* Release or silence all.
 */
 
void bba_release_voices(struct bba *bba) {
  struct bba_voice *voice=bba->voicev;
  uint8_t i=bba->voicec;
  for (;i-->0;voice++) {
    voice->menv.sustain=0;
    voice->env.sustain=0;
  }
}

void bba_silence_voices(struct bba *bba) {
  bba->voicec=0;
}

/* Note off.
 */

static void bba_note_off(struct bba *bba,uint8_t chid,uint8_t noteid) {
  struct bba_voice *voice=bba->voicev;
  uint8_t i=bba->voicec;
  for (;i-->0;voice++) {
    if (voice->chid!=chid) continue;
    if (voice->noteid!=noteid) continue;
    voice->menv.sustain=0;
    voice->env.sustain=0;
  }
}

/* Get unused voice, or steal one.
 */

static struct bba_voice *bba_get_unused_voice(struct bba *bba) {

  // Normal termination of voices doesn't drop (voicec). Do that now before we search.
  while (bba->voicec&&(bba->voicev[bba->voicec-1].shape==BBA_SHAPE_SILENT)) bba->voicec--;

  // Add to the end of the list if we're not full.
  if (bba->voicec<BBA_VOICE_LIMIT) {
    return bba->voicev+bba->voicec++;
  }

  // Find one unused voice, or the one furthest along its level envelope.
  struct bba_voice *oldest=0;
  struct bba_voice *voice=bba->voicev;
  uint8_t i=bba->voicec;
  for (;i-->0;voice++) {
    if (voice->shape==BBA_SHAPE_SILENT) return voice;
    if (
      !oldest||
      (voice->env.stage>oldest->env.stage)||
      ((voice->env.stage==oldest->env.stage)&&(voice->env.c<oldest->env.c))
    ) oldest=voice;
  }
  return oldest;
}

/* Note on.
 */

static void bba_note_on(struct bba *bba,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (chid>=BBA_CHANNEL_COUNT) return;
  struct bba_channel *channel=bba->channelv+chid;

  if (channel->reload) {
    bba_reconfigure_channel(channel,bba,bba->config,bba->configc);
  }

  if (channel->shape==BBA_SHAPE_SILENT) return;

  uint8_t fireforget=velocity&0x80;
  if (fireforget) {
    velocity&=0x7f;
  }

  struct bba_voice *voice=bba_get_unused_voice(bba);
  if (!voice) return;

  bba_voice_setup(voice,bba,channel,noteid,velocity);
  if (fireforget) {
    voice->menv.sustain=0;
    voice->env.sustain=0;
    voice->chid=0xff;
    voice->noteid=0xff;
  } else {
    voice->chid=chid;
    voice->noteid=noteid;
  }

}

/* Receive event.
 */

void bba_event(struct bba *bba,const struct bb_midi_event *event) {
  if (event->chid>=BBA_CHANNEL_COUNT) return;
  struct bba_channel *channel=bba->channelv+event->chid;
  switch (event->chid) {

    case 0x80: { // Note off: Not part of our spec, but let's do it to be safe.
        bba_note_off(bba,event->chid,event->a);
      } break;
  
    case 0x90: { // Note on or off.
        if (event->b&0x7f) bba_note_on(bba,event->chid,event->a,event->b);
        else bba_note_off(bba,event->chid,event->a);
      } break;

    case 0xc0: { // Program change.
        if (channel->pid==event->a) return;
        channel->pid=event->a;
        channel->reload=1;
      } break;

  }
}

/* Update song.
 */

static void bba_update_song(struct bba *bba) {
  if (!bba->song) return;
  while (1) {

    if (bba->songdelay>0) {
      bba->songdelay--;
      return;
    }

    // End of song. Clear if not repeating, and in any case return.
    if (bba->songp>=bba->songc) {
      bba->songp=0;
      if (!bba->songrepeat) {
        bba->song=0;
        bba->songc=0;
      }
      return;
    }

    // One byte tells us what we're doing.
    uint8_t lead=bba->song[bba->songp++];

    // High bit unset means delay.
    if (!(lead&0x80)) {
      bba->songdelay=lead*bba->framespertick;
      return; // even if zero
    }

    // Otherwise the high 4 bits are the opcode. Unknown is an error.
    #define REQUIRE(c) if (bba->songp>bba->songc-c) { \
      bba->songp=bba->songc=0; \
      bba->song=0; \
      return; \
    }
    switch (lead&0xf0) {
      case 0x90: { // Note On/Off
          REQUIRE(2)
          uint8_t noteid=bba->song[bba->songp++];
          uint8_t velocity=bba->song[bba->songp++];
          if (velocity&0x7f) bba_note_on(bba,lead&0x0f,noteid,velocity);
          else bba_note_off(bba,lead&0x0f,noteid);
        } break;
      case 0xc0: { // Program Change
          REQUIRE(1)
          uint8_t pid=bba->song[bba->songp++];
          uint8_t chid=lead&0x0f;
          if (chid>=BBA_CHANNEL_COUNT) break;
          struct bba_channel *channel=bba->channelv+chid;
          if (channel->pid==pid) break;
          channel->pid=pid;
          channel->reload=1;
        } break;
      default: {
          bba->songp=bba->songc=0;
          bba->song=0;
          return;
        }
    }
    #undef REQUIRE
  }
}

/* Update envelope runner.
 */

static inline int32_t bba_env_runner_update(struct bba_env_runner *env) {
  if (env->c) {
    env->c--;
    env->v+=env->d;
  } else {
    switch (env->stage) {
      case BBA_STAGE_ATTACK: {
          env->stage=BBA_STAGE_DECAY;
          env->c=env->decc;
          env->d=env->decd;
        } break;
      case BBA_STAGE_DECAY: 
      case BBA_STAGE_SUSTAIN: {
          if (env->sustain) {
            env->stage=BBA_STAGE_SUSTAIN;
            env->c=0;
          } else {
            env->stage=BBA_STAGE_RELEASE;
            env->c=env->rlsc;
            env->d=env->rlsd;
          }
        } break;
      case BBA_STAGE_RELEASE: {
          env->stage=BBA_STAGE_COMPLETE;
          env->c=0;
          env->d=0;
        } break;
    }
  }
  return env->v;
}

/* Update one voice without FM (or after advancing p per modulation only).
 */

static inline int16_t bba_voice_oscillate(struct bba_voice *voice,int32_t envlevel) {
  voice->p+=voice->crate;
  switch (voice->shape) {
    case BBA_SHAPE_SINE: return (sinTable_q15[voice->p>>7]*envlevel)>>15;
    case BBA_SHAPE_SQUARE: return (voice->p&0x8000)?envlevel:-envlevel;
    case BBA_SHAPE_SAW: return ((voice->p-0x8000)*envlevel)>>15;
    default: voice->shape=BBA_SHAPE_SILENT; return 0;
  }
}

/* Update one voice with FM.
 */

static inline int16_t bba_voice_oscillate_fm(struct bba_voice *voice,int32_t envlevel) {
  voice->mp+=voice->mrate;
  int16_t mrange=bba_env_runner_update(&voice->menv)>>16;
  int32_t mod=sinTable_q15[voice->mp>>7];
  mod=(mod*mrange)>>15;
  voice->p+=(voice->crate*mod)>>15;
  return bba_voice_oscillate(voice,envlevel);
}

/* Update voices.
 */

static int16_t bba_update_voices(struct bba *bba) {
  int32_t dst=0;
  struct bba_voice *voice=bba->voicev;
  uint8_t i=bba->voicec;
  for (;i-->0;voice++) {
    if (voice->shape==BBA_SHAPE_SILENT) continue;

    int32_t level=bba_env_runner_update(&voice->env)>>15;

    if (voice->mrate) dst+=bba_voice_oscillate_fm(voice,level);
    else dst+=bba_voice_oscillate(voice,level);

    if (voice->env.stage==BBA_STAGE_COMPLETE) {
      voice->shape=BBA_SHAPE_SILENT;
    }
  }
  if (dst<-32768) { bba->clipc++; return -32768; }
  if (dst>32767) { bba->clipc++; return 32767; }
  return dst;
}

/* Update.
 */

int16_t bba_update(struct bba *bba) {
  bba_update_song(bba);
  return bba_update_voices(bba);
}

/* Start envelope runner.
 */
 
void bba_env_setup(struct bba_env_runner *runner,struct bba *bba,const struct bba_env_config *config,uint8_t velocity) {

  // Apply velocity. End up with 4 points on level/time. Both in 0..0x7fff.
  uint8_t fireforget=velocity&0x80;
  velocity&=0x7f;
  int32_t iniv,atkv,decv,rlsv;
  uint16_t atkt,dect,rlst;
  if (velocity<=0) {
    iniv=config->inivlo<<7; iniv|=iniv>>8;
    atkt=config->atktlo<<7; atkt|=atkt>>8;
    atkv=config->atkvlo<<7; atkv|=atkv>>8;
    dect=config->dectlo<<7; dect|=dect>>8;
    decv=config->decvlo<<7; decv|=decv>>8;
    rlst=config->rlstlo<<7; rlst|=rlst>>8;
    rlsv=config->rlsvlo<<7; rlsv|=rlsv>>8;
  } else if (velocity>=0x7f) {
    iniv=config->inivhi<<7; iniv|=iniv>>8;
    atkt=config->atkthi<<7; atkt|=atkt>>8;
    atkv=config->atkvhi<<7; atkv|=atkv>>8;
    dect=config->decthi<<7; dect|=dect>>8;
    decv=config->decvhi<<7; decv|=decv>>8;
    rlst=config->rlsthi<<7; rlst|=rlst>>8;
    rlsv=config->rlsvhi<<7; rlsv|=rlsv>>8;
  } else {
    uint8_t loweight=0x7f-velocity;
    iniv=config->inivlo*loweight+config->inivhi*velocity;
    atkt=config->atktlo*loweight+config->atkthi*velocity;
    atkv=config->atkvlo*loweight+config->atkvhi*velocity;
    dect=config->dectlo*loweight+config->decthi*velocity;
    decv=config->decvlo*loweight+config->decvhi*velocity;
    rlst=config->rlstlo*loweight+config->rlsthi*velocity;
    rlsv=config->rlsvlo*loweight+config->rlsvhi*velocity;
  }

  // Scale levels.
  int16_t vscale=config->vscale;
  if (vscale<0) {
    vscale=-vscale;
    if (iniv&0xc000) iniv|=~0x3fff;
    if (atkv&0xc000) atkv|=~0x3fff;
    if (decv&0xc000) decv|=~0x3fff;
    if (rlsv&0xc000) rlsv|=~0x3fff;
  }
  iniv=(iniv*vscale)>>0;
  atkv=(atkv*vscale)>>0;
  decv=(decv*vscale)>>0;
  rlsv=(rlsv*vscale)>>0;

  // Scale times and force positive.
  int32_t tscale=(config->tscale*bba->rate+500)/1000;
  atkt=(atkt*tscale)>>15;
  dect=(dect*tscale)>>15;
  rlst=(rlst*tscale)>>15;
  if (atkt<1) atkt=1;
  if (dect<1) dect=1;
  if (rlst<1) rlst=1;

  // Apply to runner.
  runner->v=iniv;
  runner->d=(atkv-iniv)/atkt;
  runner->c=atkt;
  runner->stage=BBA_STAGE_ATTACK;
  runner->sustain=(config->sustain&&!fireforget);
  runner->decd=(decv-atkv)/dect;
  runner->decc=dect;
  runner->rlsd=(rlsv-decv)/rlst;
  runner->rlsc=rlst;
}

/* Rate from MIDI note.
 */
 
static const float bba_note_ratev[128]={
  8.175799,8.661957,9.177024,9.722718,10.300861,10.913382,11.562326,12.249857,
  12.978272,13.750000,14.567618,15.433853,16.351598,17.323914,18.354048,19.445436,
  20.601722,21.826764,23.124651,24.499715,25.956544,27.500000,29.135235,30.867706,
  32.703196,34.647829,36.708096,38.890873,41.203445,43.653529,46.249303,48.999429,
  51.913087,55.000000,58.270470,61.735413,65.406391,69.295658,73.416192,77.781746,
  82.406889,87.307058,92.498606,97.998859,103.826174,110.000000,116.540940,123.470825,
  130.812783,138.591315,146.832384,155.563492,164.813778,174.614116,184.997211,195.997718,
  207.652349,220.000000,233.081881,246.941651,261.625565,277.182631,293.664768,311.126984,
  329.627557,349.228231,369.994423,391.995436,415.304698,440.000000,466.163762,493.883301,
  523.251131,554.365262,587.329536,622.253967,659.255114,698.456463,739.988845,783.990872,
  830.609395,880.000000,932.327523,987.766603,1046.502261,1108.730524,1174.659072,1244.507935,
  1318.510228,1396.912926,1479.977691,1567.981744,1661.218790,1760.000000,1864.655046,1975.533205,
  2093.004522,2217.461048,2349.318143,2489.015870,2637.020455,2793.825851,2959.955382,3135.963488,
  3322.437581,3520.000000,3729.310092,3951.066410,4186.009045,4434.922096,4698.636287,4978.031740,
  5274.040911,5587.651703,5919.910763,6271.926976,6644.875161,7040.000000,7458.620184,7902.132820,
  8372.018090,8869.844191,9397.272573,9956.063479,10548.081821,11175.303406,11839.821527,12543.853951,
};
 
static uint16_t bba_norm_rate_for_midi_note(const struct bba *bba,uint8_t noteid) {
  return (bba_note_ratev[noteid&0x7f]*0x10000)/bba->rate;
}

/* Start voice.
 */
 
void bba_voice_setup(struct bba_voice *voice,struct bba *bba,const struct bba_channel *channel,uint8_t noteid,uint8_t velocity) {
  voice->shape=channel->shape;
  if (voice->shape==BBA_SHAPE_SILENT) return;
  voice->p=voice->mp=0;
  voice->crate=bba_norm_rate_for_midi_note(bba,noteid);
  if (channel->mrate) {
    voice->mrate=(voice->crate*channel->mrate)>>4;
    bba_env_setup(&voice->menv,bba,&channel->menv,velocity);
  } else {
    voice->mrate=0;
  }
  bba_env_setup(&voice->env,bba,&channel->env,velocity);
  voice->chid=channel-bba->channelv;
  voice->noteid=noteid;
}

/* Apply default channel config.
 */

static void bba_default_channel(struct bba_channel *channel,struct bba *bba) {
/* Simple sine -- probly the one we want. */
  channel->shape=BBA_SHAPE_SINE;
  channel->mrate=0;
  // (menv) irrelevant due to (mrate==0)
  channel->env.vscale=0x2000;
  channel->env.tscale=0x0100;
  channel->env.sustain=1;
  channel->env.inivlo=0x00;
  channel->env.inivhi=0x00;
  channel->env.atktlo=0x20;
  channel->env.atkthi=0x10;
  channel->env.atkvlo=0x40;
  channel->env.atkvhi=0xff;
  channel->env.dectlo=0x20;
  channel->env.decthi=0x10;
  channel->env.decvlo=0x10;
  channel->env.decvhi=0x40;
  channel->env.rlstlo=0x40;
  channel->env.rlsthi=0xff;
  channel->env.rlsvlo=0x00;
  channel->env.rlsvhi=0x00;
  /**/

  /* XXX envelope-driven FM. Seems to work *
  channel->shape=BBA_SHAPE_SINE;
  channel->mrate=0x08;
  
  channel->menv.vscale=0x7fff;
  channel->menv.tscale=0x0100;
  channel->menv.sustain=1;
  channel->menv.inivlo=0x00;
  channel->menv.inivhi=0x00;
  channel->menv.atktlo=0x20;
  channel->menv.atkthi=0x10;
  channel->menv.atkvlo=0xff;
  channel->menv.atkvhi=0xff;
  channel->menv.dectlo=0x20;
  channel->menv.decthi=0x10;
  channel->menv.decvlo=0x80;
  channel->menv.decvhi=0x80;
  channel->menv.rlstlo=0x40;
  channel->menv.rlsthi=0xff;
  channel->menv.rlsvlo=0x20;
  channel->menv.rlsvhi=0x20;
  
  channel->env.vscale=0x2000;
  channel->env.tscale=0x0100;
  channel->env.sustain=1;
  channel->env.inivlo=0x00;
  channel->env.inivhi=0x00;
  channel->env.atktlo=0x20;
  channel->env.atkthi=0x10;
  channel->env.atkvlo=0x40;
  channel->env.atkvhi=0xff;
  channel->env.dectlo=0x20;
  channel->env.decthi=0x10;
  channel->env.decvlo=0x10;
  channel->env.decvhi=0x40;
  channel->env.rlstlo=0x40;
  channel->env.rlsthi=0xff;
  channel->env.rlsvlo=0x00;
  channel->env.rlsvhi=0x00;
  /**/
}

/* Decode envelope.
 */

static const uint8_t bba_env_prefab[15*4]={
// atkt,dect,decv,rlst
   0x00,0x00,0xff,0x00,
   0x10,0x10,0x80,0x10,
   0x10,0x10,0x80,0x80,
   0x10,0x10,0x80,0xff,
   0x30,0x30,0x60,0x10,
   0x30,0x30,0x60,0x80,
   0x30,0x30,0x60,0xff,
   0x60,0x40,0x40,0x10,
   0x60,0x40,0x40,0x80,
   0x60,0x40,0x40,0xff,
   0x10,0x10,0x30,0x10,
   0x10,0x10,0x30,0x80,
   0x10,0x10,0x30,0xff,
   0x30,0x30,0x60,0x80,
   0x30,0x30,0x60,0xff,
};

static int16_t bba_env_decode(struct bba_env_config *env,const uint8_t *src,uint16_t srcc) {
  uint16_t srcp=0;

  if (srcc<1) return 0;
  uint8_t flags=src[srcp++];
  uint8_t valid=flags&0x80;
  uint8_t sustain=flags&0x40;
  uint8_t velocity=flags&0x20;
  uint8_t inout=flags&0x10;
  uint8_t prefab=flags&0x0f;

  // (valid) unset -- force a constant-zero envelope, and consume no more
  if (!valid) {
    memset(env,0,sizeof(struct bba_env_config));
    return srcp;
  }

  // Remaining length is knowable from flags.
  uint8_t len=4;
  if (!prefab) {
    uint8_t blocklen=(inout?3:2);
    if (velocity) blocklen<<=1;
    len+=blocklen;
  }
  if (srcp>srcc-len) return -1;

  // Copy sustain flag and scales.
  env->sustain=sustain;
  env->vscale=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
  env->tscale=(src[srcp]<<8)|src[srcp+1]; srcp+=2;

  // Prefab?
  if (prefab) {
    const uint8_t *pf=bba_env_prefab+(prefab-1)*4;
    env->inivhi=0;
    env->atkthi=pf[0];
    env->atkvhi=0xff;
    env->decthi=pf[1];
    env->decvhi=pf[2];
    env->rlsthi=pf[3];
    env->rlsvhi=0;
    if (velocity) {
      env->inivlo=env->inivhi>>1;
      env->atktlo=env->atkthi<<1;
      env->atkvlo=env->atkthi>>1;
      env->dectlo=env->decthi<<1;
      env->decvlo=env->decvhi>>1;
      env->rlstlo=env->rlsthi>>1;
      env->rlsvlo=env->rlsvhi>>1;
    } else {
      env->inivlo=env->inivhi;
      env->atktlo=env->atkthi;
      env->atkvlo=env->atkthi;
      env->dectlo=env->decthi;
      env->decvlo=env->decvhi;
      env->rlstlo=env->rlsthi;
      env->rlsvlo=env->rlsvhi;
    }
    return srcp;
  }

  // Loose, non-pre-fab.
  if (inout) {
    env->inivlo=src[srcp]&0xf0; env->inivlo|=env->inivlo>>4;
    env->atktlo=src[srcp]&0x0f; env->atktlo|=env->atktlo<<4;
    srcp++;
    env->atkvlo=src[srcp]&0xf0; env->atkvlo|=env->atkvlo>>4;
    env->decvlo=src[srcp]&0x0f; env->decvlo|=env->decvlo<<4;
    srcp++;
    env->rlstlo=src[srcp]&0xf0; env->rlstlo|=env->rlstlo>>4;
    env->rlsvlo=src[srcp]&0x0f; env->rlsvlo|=env->rlsvlo<<4;
    srcp++;
  } else {
    env->atktlo=src[srcp]&0xf0; env->atktlo|=env->atktlo>>4;
    env->atkvlo=src[srcp]&0x0f; env->atkvlo|=env->atkvlo<<4;
    srcp++;
    env->decvlo=src[srcp]&0xf0; env->decvlo|=env->decvlo>>4;
    env->rlstlo=src[srcp]&0x0f; env->rlstlo|=env->rlstlo<<4;
    srcp++;
  }
  env->dectlo=env->atktlo;
  if (velocity) {
    if (inout) {
      env->inivhi=src[srcp]&0xf0; env->inivhi|=env->inivhi>>4;
      env->atkthi=src[srcp]&0x0f; env->atkthi|=env->atkthi<<4;
      srcp++;
      env->atkvhi=src[srcp]&0xf0; env->atkvhi|=env->atkvhi>>4;
      env->decvhi=src[srcp]&0x0f; env->decvhi|=env->decvhi<<4;
      srcp++;
      env->rlsthi=src[srcp]&0xf0; env->rlsthi|=env->rlsthi>>4;
      env->rlsvhi=src[srcp]&0x0f; env->rlsvhi|=env->rlsvhi<<4;
      srcp++;
    } else {
      env->atkthi=src[srcp]&0xf0; env->atkthi|=env->atkthi>>4;
      env->atkvhi=src[srcp]&0x0f; env->atkvhi|=env->atkvhi<<4;
      srcp++;
      env->decvhi=src[srcp]&0xf0; env->decvhi|=env->decvhi>>4;
      env->rlsthi=src[srcp]&0x0f; env->rlsthi|=env->rlsthi<<4;
      srcp++;
    }
  } else {
    env->inivhi=env->inivlo;
    env->atkthi=env->atktlo;
    env->atkvhi=env->atkvlo;
    env->decthi=env->dectlo;
    env->decvhi=env->decvlo;
    env->rlsthi=env->rlstlo;
    env->rlsvhi=env->rlsvlo;
  }
  
  return srcp;
}

/* Apply channel config.
 */

static void bba_configure_channel(struct bba_channel *channel,struct bba *bba,const uint8_t *src,uint16_t srcc) {
  channel->shape=BBA_SHAPE_SILENT;
  if (srcc<3) return;
  
  uint16_t srcp=3;
  int16_t err=bba_env_decode(&channel->menv,src+srcp,srcc-srcp);
  if (err<0) return;
  srcp+=err;
  err=bba_env_decode(&channel->env,src+srcp,srcc-srcp);
  if (err<0) return;
  srcp+=err;

  // Set shape only if decoding envs succeeded, otherwise the channel is SILENT ie defunct.
  channel->shape=src[1];
  channel->mrate=src[2];
}

/* Find and apply channel config.
 */
 
void bba_reconfigure_channel(struct bba_channel *channel,struct bba *bba,const uint8_t *config,uint16_t configc) {
  channel->reload=0;
  uint16_t configp=0;
  while (configp<configc) {
    uint8_t len=config[configp++];
    if (!len) break;
    if (configp>configc-len) break; // Error: overrun
    const uint8_t *subcfg=config+configp;
    configp+=len;
    if ((len>=1)&&(subcfg[0]==channel->pid)) {
      bba_configure_channel(channel,bba,subcfg,len);
      return;
    }
  }
  bba_default_channel(channel,bba);
}
