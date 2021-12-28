/* bba.h
 * Low-memory synthesizer.
 * Second attempt, trying to add sine tables and maybe live MIDI decoding.
 * Dropping support for verbatim PCM -- I can't realistically see it being used.
 */

#ifndef BBA_H
#define BBA_H

#include <stdint.h>

struct bba;
struct bb_midi_event;
struct bb_midi_file;

// Client must supply this. TinyArcade has it automatically via libmcortexM0 or something like that.
extern const int16_t sinTable_q15[512];

/* Simple public interface.
 ***************************************************************************/

/* Zero the context initially.
 * Cleanup not required.
 * You must provide a nonzero rate and we don't check.
 */
void bba_init(struct bba *bba,uint16_t rate);

/* Supply instrument configuration.
 * (src) must remain in scope.
 * We do not read its content at this time.
 */
void bba_configure(struct bba *bba,const void *src,uint16_t srcc);

/* One sample at a time. That's why we don't ask for channel count -- repeat it yourself as needed.
 */
int16_t bba_update(struct bba *bba);

/* Begin playing song.
 * Noop if already playing.
 * Empty for none.
 * (src) must remain in scope.
 */
void bba_play_song(struct bba *bba,const void *src,uint16_t srcc);

void bba_event(struct bba *bba,const struct bb_midi_event *event);

void bba_release_voices(struct bba *bba);
void bba_silence_voices(struct bba *bba);

/* Details.
 ****************************************************************************/

// Arbitrary limit; can change freely up to 255.
#define BBA_VOICE_LIMIT 16

// Imposed by MIDI, but in theory you can change.
#define BBA_CHANNEL_COUNT 16

// Ticks per second. TODO Consider letting songs override this.
#define BBA_TICK_RATE 96

#define BBA_SHAPE_SILENT 0
#define BBA_SHAPE_SINE   1
#define BBA_SHAPE_SQUARE 2
#define BBA_SHAPE_SAW    3

#define BBA_STAGE_ATTACK   0
#define BBA_STAGE_DECAY    1
#define BBA_STAGE_SUSTAIN  2 /* holding */
#define BBA_STAGE_RELEASE  3
#define BBA_STAGE_COMPLETE 4 /* holding */

struct bba_env_config {
  int16_t vscale; // max v; negative for signed
  uint16_t tscale; // max t in ms
  uint8_t sustain;
  uint8_t inivlo,inivhi;
  uint8_t atktlo,atkthi;
  uint8_t atkvlo,atkvhi;
  uint8_t dectlo,decthi;
  uint8_t decvlo,decvhi;
  uint8_t rlstlo,rlsthi;
  uint8_t rlsvlo,rlsvhi;
};

struct bba_env_runner {
  int32_t v;
  int32_t d; // v delta per frame
  uint16_t c; // counts down
  uint8_t stage;
  uint8_t sustain;
  int32_t decd,rlsd;
  uint16_t decc,rlsc;
};

struct bba {
  uint16_t rate;
  uint16_t clipc;

  const uint8_t *config;
  uint16_t configc;

  const uint8_t *song;
  uint16_t songp;
  uint16_t songc;
  uint32_t songdelay;
  uint8_t songrepeat;
  uint16_t framespertick;

  struct bba_voice {
    uint8_t shape; // SILENT (0) if unused
    uint16_t p;
    uint16_t crate;
    uint16_t mp;
    uint16_t mrate; // 0 for no FM
    struct bba_env_runner menv;
    struct bba_env_runner env;
    uint8_t chid,noteid; // for identification only; (255,255) if not addressable
  } voicev[BBA_VOICE_LIMIT];
  uint8_t voicec;

  struct bba_channel {
    uint8_t pid;
    uint8_t reload; // nonzero to look up program config before next use
    uint8_t shape;
    uint8_t mrate; // u4.4
    struct bba_env_config menv;
    struct bba_env_config env;
  } channelv[BBA_CHANNEL_COUNT];
  
};

/* Voice setup does not reload channel. Caller should check for that first.
 */
void bba_env_setup(struct bba_env_runner *runner,struct bba *bba,const struct bba_env_config *config,uint8_t velocity);
void bba_voice_setup(struct bba_voice *voice,struct bba *bba,const struct bba_channel *channel,uint8_t noteid,uint8_t velocity);
void bba_reconfigure_channel(struct bba_channel *channel,struct bba *bba,const uint8_t *config,uint16_t configc);

#endif
