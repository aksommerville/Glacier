#include "multiarcade.h"
#include "bba.h"
#include <stdio.h>
#include <string.h>

extern struct ma_font font_basic;
extern struct ma_texture tex_sprites_00; // flat glacier
extern struct ma_texture tex_sprites_01; // folded glacier
extern struct ma_texture tex_sprites_02; // negative judgment
extern struct ma_texture tex_sprites_03; // positive judgment
extern struct ma_texture tex_sprites_04; // laurels
extern struct ma_texture tex_sprites_05; // track
extern struct ma_texture tex_sprites_06; // start line
extern struct ma_texture tex_sprites_07; // finish line
extern struct ma_texture tex_sprites_08; // stands
extern struct ma_texture tex_sprites_09; // tinycircuits logo
extern struct ma_texture tex_sprites_0a; // taco ad
extern struct ma_texture tex_sprites_0b; // milkshake ad
extern struct ma_texture tex_sprites_0c; // shoe ad

/* Globals.
 */

static ma_pixel_t fb_storage[96*64];
static struct ma_framebuffer fb={
  .w=96,
  .h=64,
  .stride=96,
  .v=fb_storage,
};

static ma_pixel_t bgbits_storage[96*64];
static struct ma_framebuffer bgbits={
  .w=96,
  .h=64,
  .stride=96,
  .v=bgbits_storage,
};

static uint8_t music_enable=1;
static uint8_t songid=0;
static uint16_t pvinput=0;

static uint8_t song[5120];
static uint16_t songc=0;

static uint32_t framec=0;
static uint32_t starttime=0; // framec; captures at the first input
static uint32_t endtime=0;
static uint16_t victory_blackout=0;

#define VICTORY_BLACKOUT_TIME 120

#define FINISH_LINE_X (91<<8)

#define GLACIER_MAVG_LEN 5

#define GLACIER_STATE_FLAT 0
#define GLACIER_STATE_FOLD 1

static struct glacier {
  uint8_t y; // fb pixels; constant
  uint16_t x; // 1/256 pixels (leading edge)
  uint8_t state;
  uint8_t btn_a,btn_b;
  uint32_t recent_time; // framec; zero initially
  uint8_t durv[GLACIER_MAVG_LEN];
  uint8_t durp;
  uint16_t dursum;
  uint16_t stepc;
} glacierv[2]={0};

static struct glacier *winner=0; // if set, game is over

#define JUDGMENT_LIMIT 16
#define JUDGMENT_TTL 30

static struct judgment {
  uint8_t ttl;
  int16_t x,y;
  struct ma_texture *tex;
} judgmentv[JUDGMENT_LIMIT]={0};

/* Audio.
 */

static const uint8_t bba_config[]={
  23,
    0x00, // pid
    BBA_SHAPE_SAW,
    0x08, // mrate u4.4
    // menv:
    0xf0, // valid|sustain|velocity|inout|long
      0x70,0x00, // vscale
      0x02,0x00, // tscale
      // min v
      0x84, // init level, attack time
      0xf8, // attack level, sustain level
      0x84, // release time, release level
      // max v
      0x82, // init level, attack time
      0xfc, // attack level, sustain level
      0x82, // release time, release level
    // env:
    0xe0, // valid|sustain|velocity|long
      0x10,0x00, // vscale
      0x00,0x80,
      // min v
      0x18, // attack time, attack level
      0x4c, // sustain level, release time
      // max v
      0x4f, // attack time, attack level
      0x4f, // sustain level, release time
};

struct bba synth={0};
 
int16_t audio_next() {
  return bba_update(&synth);
}

/* Begin song.
 */
 
static void restart_music() {
  bba_play_song(&synth,0,0);
  bba_release_voices(&synth);
  if (!music_enable) return; // Don't bother loading it; toggling also calls this and reloads
  char path[64];
  snprintf(path,sizeof(path),"/Glacier/song/%03d.bba",songid);
  int32_t err=ma_file_read(song,sizeof(song),path,0);
  if (err<=0) {
    songc=0;
    return;
  }
  songc=err;
  bba_play_song(&synth,song,songc);
}

static void set_song(uint8_t id) {
  if (songid==id) return;
  songid=id;
  restart_music();
}

/* Turn music on or off.
 */
 
static void toggle_music() {
  if (music_enable) {
    music_enable=0;
    bba_play_song(&synth,0,0);
    bba_release_voices(&synth);
  } else {
    music_enable=1;
    restart_music();
  }
}

/* Draw one glacier.
 */

static void draw_glacier(const struct glacier *glacier) {
  switch (glacier->state) {
    case GLACIER_STATE_FLAT: ma_blit(&fb,(glacier->x>>8)-18,glacier->y,&tex_sprites_00,0); break;
    case GLACIER_STATE_FOLD: ma_blit(&fb,(glacier->x>>8)-16,glacier->y-2,&tex_sprites_01,0); break;
  }
  if (glacier==winner) {
    ma_blit(&fb,(glacier->x>>8)-14,glacier->y-4,&tex_sprites_04,0);
  }
}

/* Render scene.
 */
 
static void render_scene(ma_pixel_t *v) {

  memcpy(fb.v,bgbits.v,fb.stride*fb.h*sizeof(ma_pixel_t));

  draw_glacier(glacierv+0);
  draw_glacier(glacierv+1);

  struct judgment *judgment=judgmentv;
  uint8_t i=JUDGMENT_LIMIT;
  for (;i-->0;judgment++) {
    if (!judgment->ttl) continue;
    ma_blit(&fb,judgment->x,judgment->y,judgment->tex,0);
  }

  if (winner) {
    uint32_t elapsed=endtime-starttime;
    uint32_t ms=(elapsed*1000)/60;
    uint32_t sec=ms/1000;
    ms%=1000;
    uint32_t min=sec/60;
    sec%=60;
    char msg[32];
    int msgc=snprintf(msg,sizeof(msg),"%d:%02d.%03d",min,sec,ms);
    ma_font_render(&fb,32,25,&font_basic,msg,msgc,0xff);
  }
}

/* Draw bgbits (once)
 */

static void draw_bgbits() {

  // Sky and grass.
  memset(bgbits.v,0xf1,96*20);
  memset(bgbits.v+96*20,0x10,96*44);

  // Tracks.
  uint8_t x=0;
  for (;x<96;x+=9) {
    ma_blit(&bgbits,x,36,&tex_sprites_05,0);
    ma_blit(&bgbits,x,51,&tex_sprites_05,0);
  }
  ma_blit(&bgbits,18,36,&tex_sprites_06,0);
  ma_blit(&bgbits,18,51,&tex_sprites_06,0);
  ma_blit(&bgbits,86,36,&tex_sprites_07,0);
  ma_blit(&bgbits,86,51,&tex_sprites_07,MA_XFORM_YREV);

  // Stands.
  for (x=0;x<96;x+=8) {
    ma_blit(&bgbits,x,4,&tex_sprites_08,0);
  }
  ma_blit(&bgbits,-2,14,&tex_sprites_09,0);
  ma_blit(&bgbits, 5,14,&tex_sprites_0a,0);
  ma_blit(&bgbits,17,14,&tex_sprites_0b,0);
  ma_blit(&bgbits,23,14,&tex_sprites_0c,0);
  ma_blit(&bgbits,35,14,&tex_sprites_09,0);
  ma_blit(&bgbits,43,14,&tex_sprites_0a,0);
  ma_blit(&bgbits,55,14,&tex_sprites_0b,0);
  ma_blit(&bgbits,61,14,&tex_sprites_0c,0);
  ma_blit(&bgbits,73,14,&tex_sprites_09,0);
  ma_blit(&bgbits,80,14,&tex_sprites_0a,0);
  ma_blit(&bgbits,92,14,&tex_sprites_0b,0);
}

/* Initial state.
 */

static void reset_glaciers() {
  memset(glacierv,0,sizeof(glacierv));
  struct glacier *top=glacierv+0,*btm=glacierv+1;
  
  top->y=35;
  top->x=19<<8;
  top->state=GLACIER_STATE_FLAT;
  top->btn_a=MA_BUTTON_A;
  top->btn_b=MA_BUTTON_B;

  btm->y=50;
  btm->x=19<<8;
  btm->state=GLACIER_STATE_FLAT;
  btm->btn_a=MA_BUTTON_UP|MA_BUTTON_LEFT;
  btm->btn_b=MA_BUTTON_DOWN|MA_BUTTON_RIGHT;

  winner=0;
  starttime=0;
  endtime=0;
}

/* Add a judgment sprite.
 */

static void judge_glacier(struct glacier *glacier,struct ma_texture *tex) {
  struct judgment *judgment=judgmentv,*q=judgmentv;
  uint8_t i=JUDGMENT_LIMIT;
  for (;i-->0;q++) {
    if (!q->ttl) {
      judgment=q;
      break;
    } else if (q->ttl<judgment->ttl) {
      judgment=q;
    }
  }
  judgment->ttl=JUDGMENT_TTL;
  judgment->x=(glacier->x>>8)-12;
  judgment->y=glacier->y-2;
  judgment->tex=tex;
}

/* Event occurred on glacier, update moving average.
 */

static uint8_t update_glacier_time(struct glacier *glacier) {
  if (!starttime) starttime=framec;
  glacier->stepc++;
  uint8_t dur=framec-glacier->recent_time;
  if (!dur) dur=1; // This is possible due to overflow. Don't let it be zero.
  glacier->recent_time=framec;
  glacier->dursum-=glacier->durv[glacier->durp];
  glacier->dursum+=dur;
  glacier->durv[glacier->durp]=dur;
  glacier->durp++;
  if (glacier->durp>=GLACIER_MAVG_LEN) glacier->durp=0;
  return dur;
}

/* Update glacier.
 */

static void update_glacier(struct glacier *glacier,uint16_t newkeys) {
  switch (glacier->state) {
  
    case GLACIER_STATE_FLAT: {
        if (newkeys&glacier->btn_a) {
          update_glacier_time(glacier);
          glacier->state=GLACIER_STATE_FOLD;
        }
      } break;

    case GLACIER_STATE_FOLD: {
        if (newkeys&glacier->btn_b) {
          uint8_t dur=update_glacier_time(glacier);
          glacier->state=GLACIER_STATE_FLAT;
          int16_t avg=glacier->dursum/GLACIER_MAVG_LEN;
          int16_t diff=dur-avg;
          if (diff<0) diff=-diff;
          int16_t score=((dur-diff)*256)/dur;
          if (score<1) score=1;
          else if (score>256) score=256;
          //ma_log("avg=%d dur=%d score=%d\n",avg,dur,score);
          glacier->x+=score;
          // Report really good and really bad strokes after the ring buffer is full.
          if (glacier->stepc>=GLACIER_MAVG_LEN) {
            if (score<150) judge_glacier(glacier,&tex_sprites_02);
            else if (score>250) judge_glacier(glacier,&tex_sprites_03);
          }
        }
      } break;
  }
}

/* Update.
 */
 
void loop() {
  framec++;

  if (victory_blackout>0) {
    victory_blackout--;
  }
  
  uint16_t input=ma_update();
  if (input!=pvinput) {
    uint16_t newkeys=(input&~pvinput);
    if (!winner) {
      update_glacier(glacierv+0,newkeys);
      update_glacier(glacierv+1,newkeys);
    } else if (!victory_blackout&&newkeys) {
      reset_glaciers();
    }
    pvinput=input;
  }

  if (!winner&&((glacierv[0].x>FINISH_LINE_X)||(glacierv[1].x>FINISH_LINE_X))) {
    endtime=framec;
    ma_log("finished in %d frames\n",endtime-starttime);
    victory_blackout=VICTORY_BLACKOUT_TIME;
    if (glacierv[0].x>glacierv[1].x) winner=glacierv+0;
    else winner=glacierv+1;
  }
  
  struct judgment *judgment=judgmentv;
  uint8_t i=JUDGMENT_LIMIT;
  for (;i-->0;judgment++) {
    if (!judgment->ttl) continue;
    judgment->ttl--;
    if (!(judgment->ttl%3)) judgment->y--;
  }
  
  render_scene(fb.v);
  ma_send_framebuffer(fb.v);
}

/* Setup.
 */

void setup() {

  bba_init(&synth,22050);
  bba_configure(&synth,bba_config,sizeof(bba_config));

  struct ma_init_params params={
    .videow=fb.w,
    .videoh=fb.h,
    .rate=60,
    .audio_rate=22050,
  };
  if (!ma_init(&params)) return;
  if ((params.videow!=fb.w)||(params.videoh!=fb.h)) return;

  draw_bgbits();
  reset_glaciers();

  srand(millis());
}
