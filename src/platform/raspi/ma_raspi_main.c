#include "ma_raspi_internal.h"
#include <signal.h>

/* Globals.
 */
 
struct ma_init_params ma_raspi_init_params={0};
uint16_t ma_raspi_input=0;
int ma_raspi_use_signals=1;
int ma_raspi_quit_requested=0;
int64_t ma_raspi_next_frame_time=0;
int64_t ma_raspi_start_time=0;
int ma_raspi_frame_skipc=0;
int ma_raspi_framec=0;
const char *ma_raspi_file_sandbox=0;
struct ma_bcm *ma_bcm=0;
struct ma_alsa *ma_alsa=0;
int ma_raspi_audio_locked=0;
struct ma_evdev *ma_evdev=0;

static volatile int ma_raspi_sigc=0;

/* Usage.
 */
 
static void ma_raspi_print_usage(const char *exename) {
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
 
static void ma_raspi_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++ma_raspi_sigc>=3) {
        fprintf(stderr," Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Cleanup.
 */
 
static void ma_raspi_quit() {

  if (ma_raspi_framec>0) {
    int64_t endtime=ma_raspi_now();
    double elapsed=(endtime-ma_raspi_start_time)/1000000.0;
    fprintf(stderr,
      "%d frames in %.03f s, average rate %.03f Hz\n",
      ma_raspi_framec,elapsed,ma_raspi_framec/elapsed
    );
  }
  
  ma_alsa_del(ma_alsa);
  ma_alsa=0;
  
  ma_bcm_del(ma_bcm);
  ma_bcm=0;
  
  ma_evdev_del(ma_evdev);
  ma_evdev=0;
}

/* Subsystem callbacks.
 */
 
static int ma_raspi_cb_button(struct ma_evdev *evdev,uint16_t btnid,int value) {
  if (value) ma_raspi_input|=btnid;
  else ma_raspi_input&=~btnid;
  return 0;
}

/* Initialize.
 */
 
static int ma_raspi_init(int argc,char **argv) {

  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    if (!strcmp(arg,"--help")) {
      ma_raspi_print_usage(argv[0]);
      return -1;
      
    } else if (!strcmp(arg,"--no-signals")) {
      ma_raspi_use_signals=0;
      
    } else if (!memcmp(arg,"--files=",8)) {
      ma_raspi_file_sandbox=arg+8;
      
    } else {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
      return -1;
    }
  }
  
  if (ma_raspi_use_signals) {
    signal(SIGINT,ma_raspi_rcvsig);
  }
  
  if (!(ma_bcm=ma_bcm_new(96,64))) return -1;
  
  if (!(ma_evdev=ma_evdev_new(ma_raspi_cb_button,0))) return -1;
  
  // We do not initialize audio yet -- wait for ma_init()
  
  setup();
  ma_raspi_start_time=ma_raspi_now();
  return 0;
}

/* Update.
 * Returns >0 to proceed.
 */
 
static int ma_raspi_update() {
  ma_raspi_framec++;
  loop();
  if (ma_raspi_audio_locked) {
    if (ma_alsa_unlock(ma_alsa)>=0) ma_raspi_audio_locked=0;
  }
  if (ma_raspi_quit_requested) return 0;
  return 1;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if (ma_raspi_init(argc,argv)<0) {
    ma_raspi_quit();
    return 1;
  }
  while (!ma_raspi_sigc) {
    int err=ma_raspi_update();
    if (err<0) {
      ma_raspi_quit();
      return 1;
    }
    if (!err) break;
  }
  ma_raspi_quit();
  return 0;
}
