/* bb_midi.h
 * Decode MIDI files and streams.
 */
 
#ifndef BB_MIDI_H
#define BB_MIDI_H

#include <stdint.h>

/* Event.
 *****************************************************************/
 
// Channel Voice Messages.
#define BB_MIDI_OPCODE_NOTE_OFF     0x80
#define BB_MIDI_OPCODE_NOTE_ON      0x90
#define BB_MIDI_OPCODE_NOTE_ADJUST  0xa0
#define BB_MIDI_OPCODE_CONTROL      0xb0
#define BB_MIDI_OPCODE_PROGRAM      0xc0
#define BB_MIDI_OPCODE_PRESSURE     0xd0
#define BB_MIDI_OPCODE_WHEEL        0xe0

// System Common Messages. (stream only)
#define BB_MIDI_OPCODE_TIMECODE      0xf1
#define BB_MIDI_OPCODE_SONG_POSITION 0xf2
#define BB_MIDI_OPCODE_SONG_SELECT   0xf3
#define BB_MIDI_OPCODE_TUNE_REQUEST  0xf6
#define BB_MIDI_OPCODE_EOX           0xf7

// Real Time Messages. (stream only)
#define BB_MIDI_OPCODE_CLOCK         0xf8
#define BB_MIDI_OPCODE_START         0xfa
#define BB_MIDI_OPCODE_CONTINUE      0xfb
#define BB_MIDI_OPCODE_STOP          0xfc
#define BB_MIDI_OPCODE_ACTIVE_SENSE  0xfe
#define BB_MIDI_OPCODE_SYSTEM_RESET  0xff

// Other Messages.
#define BB_MIDI_OPCODE_SYSEX         0xf0 /* stream or file. You'll get this only for complete events. */
#define BB_MIDI_OPCODE_META          0x01 /* file only. (a)=type */
#define BB_MIDI_OPCODE_PARTIAL       0x02 /* dump of undecodable data, eg Sysex interrupted by Realtime */
#define BB_MIDI_OPCODE_STALL         0x03 /* next event not fully received yet */

#define BB_MIDI_CHID_ALL 0xff

// Channel Mode Messages. (Control keys)
#define BB_MIDI_CONTROL_SOUND_OFF     0x78
#define BB_MIDI_CONTROL_RESET_CONTROL 0x79
#define BB_MIDI_CONTROL_LOCAL         0x7a
#define BB_MIDI_CONTROL_NOTES_OFF     0x7b /* 0x7b..0x7f are all "notes off" */
#define BB_MIDI_CONTROL_OMNI_OFF      0x7c
#define BB_MIDI_CONTROL_OMNI_ON       0x7d
#define BB_MIDI_CONTROL_MONOPHONIC    0x7e
#define BB_MIDI_CONTROL_POLYPHONIC    0x7f

struct bb_midi_event {
  uint8_t opcode;
  uint8_t chid;
  uint8_t a,b;
  const void *v;
  int c;
};

/* Stream.
 *********************************************************/
 
struct bb_midi_stream {
  uint8_t status; // CV opcode+chid, MIDI_OPCODE_SYSEX, or zero
  uint8_t d[2];
  int dc;
};

/* Consume some amount of (src), populate (event), return the length consumed.
 * We do not support Channel Voice events split by Realtime.
 */
int bb_midi_stream_decode(struct bb_midi_event *event,struct bb_midi_stream *stream,const void *src,int srcc);

/* Helper to track state for multiple streams, keyed by (driver,devid).
 */
struct bb_midi_intake {
  struct bb_midi_intake_device {
    const void *driver;
    int devid;
    struct bb_midi_stream stream;
  } *devicev;
  int devicec,devicea;
};

void bb_midi_intake_cleanup(struct bb_midi_intake *intake);

struct bb_midi_stream *bb_midi_intake_get_stream(const struct bb_midi_intake *intake,const void *driver,int devid);
struct bb_midi_stream *bb_midi_intake_add_stream(struct bb_midi_intake *intake,const void *driver,int devid);
void bb_midi_intake_remove_stream(struct bb_midi_intake *intake,const void *driver,int devid);

/* File.
 ********************************************************/
 
struct bb_midi_file {
  int refc;
  
  int format;
  int track_count;
  int division;
  
  struct bb_midi_track {
    uint8_t *v;
    int c;
  } *trackv;
  int trackc,tracka;
};

struct bb_midi_file_reader {
  int refc;
  
  int repeat;
  int rate;
  int usperqnote,usperqnote0;
  int delay; // frames
  double framespertick;
  
  struct bb_midi_file *file;
  struct bb_midi_track_reader {
    struct bb_midi_track track; // (v) is borrowed
    int p,p0;
    int term,term0;
    int delay,delay0; // <0 if not decoded yet; >0 in frames
    uint8_t status,status0;
  } *trackv;
  int trackc;
};

void bb_midi_file_del(struct bb_midi_file *file);
int bb_midi_file_ref(struct bb_midi_file *file);
struct bb_midi_file *bb_midi_file_new(const void *src,int srcc);

void bb_midi_file_reader_del(struct bb_midi_file_reader *reader);
int bb_midi_file_reader_ref(struct bb_midi_file_reader *reader);
struct bb_midi_file_reader *bb_midi_file_reader_new(struct bb_midi_file *file,int rate);

int bb_midi_file_reader_set_rate(struct bb_midi_file_reader *reader,int rate);

/* Read the next event from this file, if it is not delayed.
 * Returns:
 *   >0: No event. Return value is frame count to next event.
 *   <0: No event. End of file or error.
 *    0: Event produced.
 * When it returns zero, you should process the event and immediately call again.
 * When >0, we will continue returning the same thing until you "advance" the clock.
 * In "repeat" mode, there is a brief implicit delay at the end of the file.
 */
int bb_midi_file_reader_update(struct bb_midi_event *event,struct bb_midi_file_reader *reader);

/* Advance the reader's clock by the given frame count.
 * If this is greater than the delay to the next event, we report an error.
 */
int bb_midi_file_reader_advance(struct bb_midi_file_reader *reader,int framec);

// To distinguish errors from EOF.
int bb_midi_file_reader_is_complete(const struct bb_midi_file_reader *reader);

#endif
