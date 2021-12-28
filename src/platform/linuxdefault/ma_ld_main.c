#include "ma_ld_internal.h"
#include <signal.h>

/* Globals.
 */
 
struct ma_init_params ma_ld_init_params={0};
uint16_t ma_ld_input=0;
int ma_ld_use_signals=1;
int ma_ld_quit_requested=0;
int64_t ma_ld_next_frame_time=0;
int64_t ma_ld_start_time=0;
int ma_ld_frame_skipc=0;
int ma_ld_framec=0;
const char *ma_ld_file_sandbox=0;
struct ma_x11 *ma_x11=0;
struct ma_pulse *ma_pulse=0;
int ma_ld_audio_locked=0;
struct ma_evdev *ma_evdev=0;

static volatile int ma_ld_sigc=0;

/* Usage.
 */
 
static void ma_ld_print_usage(const char *exename) {
  if (!exename||!exename[0]) exename="multiarcade";
  fprintf(stderr,"\nUsage: %s [OPTIONS]\n",exename);
  fprintf(stderr,
    "OPTIONS:\n"
    "  --no-signals      Don't react to SIGINT.\n"
    "  --files=PATH      [] Let the app access files under the given directory.\n"
    "\n"
  );
}

/* Signal handler.
 */
 
static void ma_ld_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++ma_ld_sigc>=3) {
        fprintf(stderr," Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Cleanup.
 */
 
static void ma_ld_quit() {

  if (ma_ld_framec>0) {
    int64_t endtime=ma_ld_now();
    double elapsed=(endtime-ma_ld_start_time)/1000000.0;
    fprintf(stderr,
      "%d frames in %.03f s, average rate %.03f Hz\n",
      ma_ld_framec,elapsed,ma_ld_framec/elapsed
    );
  }
  
  ma_pulse_del(ma_pulse);
  ma_pulse=0;
  
  ma_x11_del(ma_x11);
  ma_x11=0;
  
  ma_evdev_del(ma_evdev);
  ma_evdev=0;
}

/* Subsystem callbacks.
 */
 
static int ma_ld_cb_button(struct ma_x11 *DONT_TOUCH,uint16_t btnid,int value) {
  // x11 and evdev have the same button callback shape, and this is both.
  if (value) ma_ld_input|=btnid;
  else ma_ld_input&=~btnid;
  return 0;
}

static int ma_ld_cb_close(struct ma_x11 *x11) {
  ma_ld_quit_requested=1;
  return 0;
}

/* Initialize.
 */
 
static int ma_ld_init(int argc,char **argv) {

  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    if (!strcmp(arg,"--help")) {
      ma_ld_print_usage(argv[0]);
      return -1;
      
    } else if (!strcmp(arg,"--no-signals")) {
      ma_ld_use_signals=0;
      
    } else if (!memcmp(arg,"--files=",8)) {
      ma_ld_file_sandbox=arg+8;
      
    } else {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
      return -1;
    }
  }
  
  if (ma_ld_use_signals) {
    signal(SIGINT,ma_ld_rcvsig);
  }
  
  if (!(ma_x11=ma_x11_new(
    MA_APP_NAME,96,64,0,
    ma_ld_cb_button,ma_ld_cb_close,0
  ))) return -1;
  
  if (!(ma_evdev=ma_evdev_new((void*)ma_ld_cb_button,0))) {
    fprintf(stderr,"Failed to initialize evdev (joysticks). Proceeding without...\n");
  }
  
  // We do not initialize audio yet -- wait for ma_init()
  
  setup();
  ma_ld_start_time=ma_ld_now();
  return 0;
}

/* Update.
 * Returns >0 to proceed.
 */
 
static int ma_ld_update() {
  ma_ld_framec++;
  loop();
  if (ma_ld_audio_locked) {
    if (ma_pulse_unlock(ma_pulse)>=0) ma_ld_audio_locked=0;
  }
  if (ma_ld_quit_requested) return 0;
  return 1;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if (ma_ld_init(argc,argv)<0) {
    ma_ld_quit();
    return 1;
  }
  while (!ma_ld_sigc) {
    int err=ma_ld_update();
    if (err<0) {
      ma_ld_quit();
      return 1;
    }
    if (!err) break;
  }
  ma_ld_quit();
  return 0;
}

//XXX TEMP
void usb_begin() {}
void usb_send(const void *v,int c) {}
int usb_read(void *dst,int dsta) { return 0; }
int usb_read_byte() { return -1; }
