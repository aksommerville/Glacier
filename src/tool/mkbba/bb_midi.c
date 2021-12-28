#include "bb_midi.h"
#include "common/encoder.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

/* Replace Note On at velocity zero, with Note Off at velocity 0x40.
 */
 
static inline void bb_midi_note_off_if_v0_note_on(struct bb_midi_event *event) {
  if (event->opcode==BB_MIDI_OPCODE_NOTE_ON) {
    if (event->b==0x00) {
      event->opcode=BB_MIDI_OPCODE_NOTE_OFF;
      event->b=0x40;
    }
  }
}

/* Decode event from stream.
 */
 
int bb_midi_stream_decode(struct bb_midi_event *event,struct bb_midi_stream *stream,const void *src,int srcc) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  
  // Consume Status byte if there is one.
  // If we had an incomplete event pending, it's lost.
  int expectc;
  switch (SRC[srcp]&0xf0) {
    case BB_MIDI_OPCODE_NOTE_ON: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_NOTE_OFF: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_NOTE_ADJUST: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_CONTROL: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_PROGRAM: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_PRESSURE: stream->status=SRC[srcp++]; stream->dc=0; break;
    case BB_MIDI_OPCODE_WHEEL: stream->status=SRC[srcp++]; stream->dc=0; break;
    case 0xf0: switch (SRC[srcp]) {
      case BB_MIDI_OPCODE_SYSEX: stream->status=SRC[srcp++]; stream->dc=0; break;
      case BB_MIDI_OPCODE_EOX: { // End Of Exclusive behaves like Realtime, but resets Status.
          stream->status=0;
          stream->dc=0;
          event->opcode=SRC[srcp++];
          event->chid=BB_MIDI_CHID_ALL;
        } return srcp;
      case BB_MIDI_OPCODE_CLOCK: // realtime...
      case BB_MIDI_OPCODE_START:
      case BB_MIDI_OPCODE_CONTINUE:
      case BB_MIDI_OPCODE_STOP:
      case BB_MIDI_OPCODE_ACTIVE_SENSE:
      case BB_MIDI_OPCODE_SYSTEM_RESET: {
          event->opcode=SRC[srcp++];
          event->chid=BB_MIDI_CHID_ALL;
        } return srcp;
      default: { // Unsupported opcode, probably System Common. Consume whatever data bytes there are after it.
          stream->status=0;
          stream->dc=0;
          event->opcode=SRC[srcp++];
          event->chid=BB_MIDI_CHID_ALL;
          event->v=SRC+srcp;
          event->c=0;
          while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { srcp++; event->c++; }
          if (event->c>=2) { event->a=((uint8_t*)event->v)[0]; event->b=((uint8_t*)event->v)[1]; }
          else if (event->c>=1) { event->a=((uint8_t*)event->v)[0]; }
          return srcp;
        }
      } break;
  }
  
  // If we're doing Sysex, consume as much as possible.
  // Report it as SYSEX event if the terminator is already present, otherwise PARTIAL.
  if (stream->status==BB_MIDI_OPCODE_SYSEX) {
    event->opcode=BB_MIDI_OPCODE_PARTIAL;
    event->chid=BB_MIDI_CHID_ALL;
    event->v=SRC+srcp;
    event->c=0;
    while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { event->c++; srcp++; }
    if ((srcp<srcc)&&(SRC[srcp]==BB_MIDI_OPCODE_EOX)) {
      stream->status=0;
      event->opcode=BB_MIDI_OPCODE_SYSEX;
      srcp++;
    }
    return srcp;
  }
  
  // If we don't have a status, something is wrong. Report a PARTIAL with data bytes.
  if (!stream->status) {
    event->opcode=BB_MIDI_OPCODE_PARTIAL;
    event->chid=BB_MIDI_CHID_ALL;
    event->v=SRC+srcp;
    event->c=0;
    while ((srcp<srcc)&&!(SRC[srcp]&0x80)) { event->c++; srcp++; }
    return srcp;
  }
  
  // Determine how many data bytes we're looking for, based on Status.
  int datac;
  switch (stream->status&0xf0) {
    case BB_MIDI_OPCODE_NOTE_OFF: datac=2; break;
    case BB_MIDI_OPCODE_NOTE_ON: datac=2; break;
    case BB_MIDI_OPCODE_NOTE_ADJUST: datac=2; break;
    case BB_MIDI_OPCODE_CONTROL: datac=2; break;
    case BB_MIDI_OPCODE_PROGRAM: datac=1; break;
    case BB_MIDI_OPCODE_PRESSURE: datac=1; break;
    case BB_MIDI_OPCODE_WHEEL: datac=2; break;
    default: return -1;
  }
  while (stream->dc<datac) {
    if ((srcp>=srcc)||(SRC[srcp]&0x80)) {
      event->opcode=BB_MIDI_OPCODE_STALL;
      return srcp;
    }
    stream->d[stream->dc++]=SRC[srcp++];
  }
  event->opcode=stream->status&0xf0;
  event->chid=stream->status&0x0f;
  event->a=stream->d[0];
  event->b=stream->d[1];
  stream->dc=0;
  bb_midi_note_off_if_v0_note_on(event);
  return srcp;
}

/* Delete.
 */
 
void bb_midi_file_del(struct bb_midi_file *file) {
  if (!file) return;
  if (file->refc-->1) return;
  if (file->trackv) {
    while (file->trackc-->0) {
      void *v=file->trackv[file->trackc].v;
      if (v) free(v);
    }
    free(file->trackv);
  }
  free(file);
}

/* Retain.
 */
 
int bb_midi_file_ref(struct bb_midi_file *file) {
  if (!file) return -1;
  if (file->refc<1) return -1;
  if (file->refc==INT_MAX) return -1;
  file->refc++;
  return 0;
}

/* Receive MThd.
 */
 
static int bb_midi_file_decode_MThd(struct bb_midi_file *file,const uint8_t *src,int srcc) {

  if (file->division) return -1; // Multiple MThd
  if (srcc<6) return -1; // Malformed MThd
  
  file->format=(src[0]<<8)|src[1];
  file->track_count=(src[2]<<8)|src[3];
  file->division=(src[4]<<8)|src[5];
  
  if (!file->division) return -1; // Division must be nonzero, both mathematically and because we use it as "MThd present" flag.
  if (file->division&0x8000) return -1; // SMPTE timing not supported (TODO?)
  // (format) must be 1, but I think we can take whatever.

  return 0;
}

/* Receive MTrk.
 */
 
static int bb_midi_file_decode_MTrk(struct bb_midi_file *file,const void *src,int srcc) {

  if (file->trackc>=file->tracka) {
    int na=file->tracka+8;
    if (na>INT_MAX/sizeof(struct bb_midi_track)) return -1;
    void *nv=realloc(file->trackv,sizeof(struct bb_midi_track)*na);
    if (!nv) return -1;
    file->trackv=nv;
    file->tracka=na;
  }
  
  void *nv=malloc(srcc?srcc:1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  
  struct bb_midi_track *track=file->trackv+file->trackc++;
  track->v=nv;
  track->c=srcc;

  return 0;
}

/* Receive unknown chunk.
 */
 
static int bb_midi_file_decode_other(struct bb_midi_file *file,int chunkid,const void *src,int srcc) {
  // Would be reasonable to call it an error. Whatever.
  return 0;
}

/* Finish decoding, validate.
 */
 
static int bb_midi_file_decode_finish(struct bb_midi_file *file) {
  if (!file->division) return -1; // No MThd
  if (file->trackc<1) return -1; // No MTrk
  return 0;
}

/* Decode.
 */

static int bb_midi_file_decode(struct bb_midi_file *file,const void *src,int srcc) {
  struct decoder decoder={.v=src,.c=srcc};
  while (decoder_remaining(&decoder)) {
    int chunkid,c;
    const void *v;
    if (decode_32be(&chunkid,&decoder)<0) return -1;
    if (decode_32be(&c,&decoder)<0) return -1;
    if ((c<0)||(c>decoder_remaining(&decoder))) return -1;
    v=decoder.v+decoder.p;
    decoder.p+=c;
    switch (chunkid) {
      case ('M'<<24)|('T'<<16)|('h'<<8)|'d': if (bb_midi_file_decode_MThd(file,v,c)<0) return -1; break;
      case ('M'<<24)|('T'<<16)|('r'<<8)|'k': if (bb_midi_file_decode_MTrk(file,v,c)<0) return -1; break;
      default: if (bb_midi_file_decode_other(file,chunkid,v,c)<0) return -1;
    }
  }
  return bb_midi_file_decode_finish(file);
}

/* New.
 */
 
struct bb_midi_file *bb_midi_file_new(const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return 0;
  struct bb_midi_file *file=calloc(1,sizeof(struct bb_midi_file));
  if (!file) return 0;
  file->refc=1;
  if (bb_midi_file_decode(file,src,srcc)<0) {
    bb_midi_file_del(file);
    return 0;
  }
  return file;
}

/* Object lifecycle.
 */
 
void bb_midi_file_reader_del(struct bb_midi_file_reader *reader) {
  if (!reader) return;
  if (reader->refc-->1) return;
  bb_midi_file_del(reader->file);
  if (reader->trackv) free(reader->trackv);
  free(reader);
}

int bb_midi_file_reader_ref(struct bb_midi_file_reader *reader) {
  if (!reader) return -1;
  if (reader->refc<1) return -1;
  if (reader->refc==INT_MAX) return -1;
  reader->refc++;
  return 0;
}

/* Initialize tracks.
 */
 
static int bb_midi_file_reader_init_tracks(struct bb_midi_file_reader *reader) {
  reader->trackc=reader->file->trackc;
  if (!(reader->trackv=calloc(sizeof(struct bb_midi_track_reader),reader->trackc))) return -1;
  const struct bb_midi_track *ktrack=reader->file->trackv;
  struct bb_midi_track_reader *rtrack=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;ktrack++,rtrack++) {
    rtrack->track=*ktrack;
    rtrack->delay=rtrack->delay0=-1;
  }
  return 0;
}

/* New.
 */
 
struct bb_midi_file_reader *bb_midi_file_reader_new(struct bb_midi_file *file,int rate) {
  if (rate<1) return 0;
  struct bb_midi_file_reader *reader=calloc(1,sizeof(struct bb_midi_file_reader));
  if (!reader) return 0;
  
  reader->refc=1;
  reader->rate=0;
  reader->repeat=0;
  reader->usperqnote=500000;
  reader->usperqnote0=reader->usperqnote;
  
  if (bb_midi_file_ref(file)<0) {
    bb_midi_file_reader_del(reader);
    return 0;
  }
  reader->file=file;
  
  if (bb_midi_file_reader_init_tracks(reader)<0) {
    bb_midi_file_reader_del(reader);
    return 0;
  }
  
  if (bb_midi_file_reader_set_rate(reader,rate)<0) {
    bb_midi_file_reader_del(reader);
    return 0;
  }
  
  return reader;
}

/* Recalculate tempo derivative fields from (rate,usperqnote).
 */
 
static void bb_midi_file_reader_recalculate_tempo(struct bb_midi_file_reader *reader) {
  // We could combine (rate/(division*M)) into one coefficient that only changes with rate,
  // but really tempo changes aren't going to happen often enough to worry about it.
  reader->framespertick=((double)reader->usperqnote*(double)reader->rate)/((double)reader->file->division*1000000.0);
}

/* Change rate.
 */

int bb_midi_file_reader_set_rate(struct bb_midi_file_reader *reader,int rate) {
  if (rate<1) return -1;
  if (rate==reader->rate) return 0;
  reader->rate=rate;
  bb_midi_file_reader_recalculate_tempo(reader);
  return 0;
}

/* Return to repeat point and return new delay (always >0).
 */
 
static int bb_midi_file_a_capa(struct bb_midi_file_reader *reader) {
  if (reader->usperqnote!=reader->usperqnote0) {
    reader->usperqnote=reader->usperqnote0;
    bb_midi_file_reader_recalculate_tempo(reader);
  }
  int delay=INT_MAX;
  struct bb_midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    track->p=track->p0;
    track->term=track->term0;
    track->delay=track->delay0;
    track->status=track->status0;
    if ((track->delay>=0)&&(track->delay<delay)) delay=track->delay;
  }
  if (delay<1) {
    for (track=reader->trackv,i=reader->trackc;i-->0;track++) {
      if (track->delay>=0) track->delay++;
    }
    return 1;
  }
  return delay;
}

/* Set the repeat point from current state.
 */
 
static void bb_midi_file_reader_set_capa(struct bb_midi_file_reader *reader) {
  reader->usperqnote0=reader->usperqnote;
  struct bb_midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    track->p0=track->p;
    track->term0=track->term;
    track->delay0=track->delay;
    track->status0=track->status;
  }
}

/* Examine an event before returning it, and update any reader state, eg tempo.
 * We can cause errors but can not modify the event or prevent it being returned, that's by design.
 */
 
static int bb_midi_file_reader_local_event(struct bb_midi_file_reader *reader,const struct bb_midi_event *event) {
  switch (event->opcode) {
    case BB_MIDI_OPCODE_META: switch (event->a) {
    
        case 0x51: { // Set Tempo
            if (event->c==3) {
              const uint8_t *B=event->v;
              int usperqnote=(B[0]<<16)|(B[1]<<8)|B[2];
              if (usperqnote&&(usperqnote!=reader->usperqnote)) {
                reader->usperqnote=usperqnote;
                bb_midi_file_reader_recalculate_tempo(reader);
              }
            }
          } break;
          
        // We can't do 0x2f End of Track because we don't know anymore which track caused it.
        // I feel 0x2f is redundant anyway.
          
      } break;
    case BB_MIDI_OPCODE_SYSEX: {
    
        // Highly nonstandard loop declaration, it's my own thing.
        if ((event->c==9)&&!memcmp(event->v,"BBx:START",9)) {
          bb_midi_file_reader_set_capa(reader);
        }
    
      } break;
  }
  return 0;
}

/* Read delay from one track.
 * Sets (term) at EOF or error, otherwise sets (delay>=0).
 * Call when (delay<0).
 */
 
static void bb_midi_track_read_delay(struct bb_midi_file_reader *reader,struct bb_midi_track_reader *track) {
  if (track->p>=track->track.c) {
    track->term=1;
    return;
  }
  int err=vlq_decode(&track->delay,track->track.v+track->p,track->track.c-track->p);
  if ((err<=0)||(track->delay<0)) {
    track->term=1;
    return;
  }
  track->p+=err;
  if (track->delay) { // zero is zero no matter what
    // ...and nonzero is nonzero no matter what
    int framec=lround(track->delay*reader->framespertick);
    if (framec<1) track->delay=1;
    else track->delay=framec;
  }
}

/* Read one event (after delay).
 * On success, returns >0 and fully updates track.
 * Anything else, caller should mark the track terminated and stop using it.
 * This does not change any reader state, only track state.
 */
 
static int bb_midi_track_read_event(struct bb_midi_event *event,struct bb_midi_file_reader *reader,struct bb_midi_track_reader *track) {
  
  // EOF?
  if (track->p>=track->track.c) return -1;
  
  // Acquire status byte.
  uint8_t status=track->status;
  if (track->track.v[track->p]&0x80) {
    status=track->track.v[track->p++];
  }
  if (!status) return -1;
  track->status=status;
  track->delay=-1;
  
  event->opcode=status&0xf0;
  event->chid=status&0x0f;
  switch (status&0xf0) {
    #define DATA_A { \
      if (track->p>track->track.c-1) return -1; \
      event->a=track->track.v[track->p++]; \
    }
    #define DATA_AB { \
      if (track->p>track->track.c-2) return -1; \
      event->a=track->track.v[track->p++]; \
      event->b=track->track.v[track->p++]; \
    }
    case BB_MIDI_OPCODE_NOTE_OFF: DATA_AB break;
    case BB_MIDI_OPCODE_NOTE_ON: DATA_AB if (!event->b) { event->opcode=BB_MIDI_OPCODE_NOTE_OFF; event->b=0x40; } break;
    case BB_MIDI_OPCODE_NOTE_ADJUST: DATA_AB break;
    case BB_MIDI_OPCODE_CONTROL: DATA_AB break;
    case BB_MIDI_OPCODE_PROGRAM: DATA_A break;
    case BB_MIDI_OPCODE_PRESSURE: DATA_A break;
    case BB_MIDI_OPCODE_WHEEL: DATA_AB break;
    #undef DATA_A
    #undef DATA_AB
    default: track->status=0; event->chid=BB_MIDI_CHID_ALL; switch (status) {
    
        case 0xf0: case 0xf7: {
            event->opcode=BB_MIDI_OPCODE_SYSEX;
            int len,err;
            if ((err=vlq_decode(&len,track->track.v+track->p,track->track.c-track->p))<1) return -1;
            track->p+=err;
            if (track->p>track->track.c-len) return -1;
            event->v=track->track.v+track->p;
            event->c=len;
            track->p+=len;
            if ((event->c>=1)&&(((uint8_t*)event->v)[event->c-1]==0xf7)) event->c--;
          } return 1;
    
        case 0xff: {
            event->opcode=BB_MIDI_OPCODE_META;
            if (track->p>=track->track.c) return -1;
            event->a=track->track.v[track->p++];
            int len,err;
            if ((err=vlq_decode(&len,track->track.v+track->p,track->track.c-track->p))<1) return -1;
            track->p+=err;
            if (track->p>track->track.c-len) return -1;
            event->v=track->track.v+track->p;
            event->c=len;
            track->p+=len;
          } return 1;
    
        default: return -1;
      }
  }
  
  return 1;
}

/* Next event, main entry point.
 */
 
int bb_midi_file_reader_update(struct bb_midi_event *event,struct bb_midi_file_reader *reader) {

  // Already delaying? Get out.
  if (reader->delay) return reader->delay;

  // One event at time zero, or determine the delay to the next one.
  int delay=INT_MAX;
  struct bb_midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (track->term) continue;
    if (track->delay<0) {
      bb_midi_track_read_delay(reader,track);
      if (track->term) continue;
    }
    if (track->delay) {
      if (track->delay<delay) delay=track->delay;
      continue;
    }
    if (bb_midi_track_read_event(event,reader,track)>0) {
      if (bb_midi_file_reader_local_event(reader,event)<0) return -1;
      return 0;
    } else {
      track->term=1;
    }
  }
  
  // If we didn't select a delay, we're at EOF.
  if (delay==INT_MAX) {
    if (reader->repeat) {
      fprintf(stderr,"--- song repeat ---\n");
      return bb_midi_file_a_capa(reader);
    }
    return -1;
  }
  
  // Sleep.
  reader->delay=delay;
  return delay;
}

/* Advance clock.
 */
 
int bb_midi_file_reader_advance(struct bb_midi_file_reader *reader,int framec) {
  if (framec<0) return -1;
  if (framec>reader->delay) return -1;
  reader->delay-=framec;
  struct bb_midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (track->delay>=framec) track->delay-=framec;
  }
  return 0;
}

/* Check reader completion.
 */
 
int bb_midi_file_reader_is_complete(const struct bb_midi_file_reader *reader) {
  if (!reader) return 0;
  const struct bb_midi_track_reader *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    if (!track->term) return 0;
  }
  return 1;
}

/* Intake.
 */

void bb_midi_intake_cleanup(struct bb_midi_intake *intake) {
  if (intake->devicev) free(intake->devicev);
  memset(intake,0,sizeof(struct bb_midi_intake));
}

static int bb_midi_intake_search(const struct bb_midi_intake *intake,const void *driver,int devid) {
  int lo=0,hi=intake->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct bb_midi_intake_device *device=intake->devicev+ck;
         if (driver<device->driver) hi=ck;
    else if (driver>device->driver) lo=ck+1;
    else if (devid<device->devid) hi=ck;
    else if (devid>device->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct bb_midi_stream *bb_midi_intake_get_stream(const struct bb_midi_intake *intake,const void *driver,int devid) {
  int p=bb_midi_intake_search(intake,driver,devid);
  if (p<0) return 0;
  return &(intake->devicev[p].stream);
}

struct bb_midi_stream *bb_midi_intake_add_stream(struct bb_midi_intake *intake,const void *driver,int devid) {
  int p=bb_midi_intake_search(intake,driver,devid);
  if (p>=0) return &intake->devicev[p].stream;
  p=-p-1;
  if (intake->devicec>=intake->devicea) {
    int na=intake->devicea+4;
    if (na>INT_MAX/sizeof(struct bb_midi_intake_device)) return 0;
    void *nv=realloc(intake->devicev,sizeof(struct bb_midi_intake_device)*na);
    if (!nv) return 0;
    intake->devicev=nv;
    intake->devicea=na;
  }
  struct bb_midi_intake_device *device=intake->devicev+p;
  memmove(device+1,device,sizeof(struct bb_midi_intake_device)*(intake->devicec-p));
  intake->devicec++;
  memset(device,0,sizeof(struct bb_midi_intake_device));
  device->driver=driver;
  device->devid=devid;
  return &device->stream;
}

void bb_midi_intake_remove_stream(struct bb_midi_intake *intake,const void *driver,int devid) {
  int p=bb_midi_intake_search(intake,driver,devid);
  if (p<0) return;
  struct bb_midi_intake_device *device=intake->devicev+p;
  intake->devicec--;
  memmove(device,device+1,sizeof(struct bb_midi_intake_device)*(intake->devicec-p));
}
