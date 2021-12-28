#include "ma_generic_internal.h"
#include <termios.h>
#include <unistd.h>
#include <signal.h>

/* Globals.
 */
 
struct ma_init_params ma_generic_init_params={0};
uint16_t ma_generic_input=0;
int ma_generic_use_signals=1;
int ma_generic_use_stdin=1;
int ma_generic_use_stdout=1;
int ma_generic_quit_requested=0;
int64_t ma_generic_next_frame_time=0;
int64_t ma_generic_start_time=0;
int ma_generic_frame_skipc=0;
int ma_generic_framec=0;
const char *ma_generic_file_sandbox=0;
int ma_generic_keyhold[6]={0};
int ma_generic_keyhold_time=30;
uint16_t ma_generic_input_next=0;

static int ma_generic_gotstdin=0;
static struct termios ma_generic_termios={0};
static volatile int ma_generic_sigc=0;

/* Usage.
 */
 
static void ma_generic_print_usage(const char *exename) {
  if (!exename||!exename[0]) exename="multiarcade";
  fprintf(stderr,"\nUsage: %s [OPTIONS]\n",exename);
  fprintf(stderr,
    "OPTIONS:\n"
    "  --no-signals      Don't react to SIGINT.\n"
    "  --no-stdin        Don't read input from stdin.\n"
    "  --no-stdout       Don't emit video to stdout.\n"
    "  --keyhold=FRAMEC  [30] Hold input events ON for so many frames. Should slightly exceed your key-repeat initial delay.\n"
    "  --files=PATH      [] Let the app access files under the given directory.\n"
    "\n"
  );
}

/* Signal handler.
 */
 
static void ma_generic_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++ma_generic_sigc>=3) {
        fprintf(stderr,"\n\x1b[0m Too many unprocessed signals.\n");
        if (ma_generic_gotstdin) {
          tcsetattr(STDIN_FILENO,TCSANOW,&ma_generic_termios);
        }
        exit(1);
      } break;
    default: fprintf(stderr,"SIGNAL %d\n",sigid);
  }
}

/* Cleanup.
 */
 
static void ma_generic_quit() {

  if (ma_generic_framec>0) {
    int64_t endtime=ma_generic_now();
    double elapsed=(endtime-ma_generic_start_time)/1000000.0;
    fprintf(stderr,
      "%d frames in %.03f s, average rate %.03f Hz\n",
      ma_generic_framec,elapsed,ma_generic_framec/elapsed
    );
    if (ma_generic_use_stdout) usleep(2000000); // using stdout for video, assume our caller clears at termination.
  }

  if (ma_generic_gotstdin) {
    // Super important to restore termios mode.
    tcsetattr(STDIN_FILENO,TCSANOW,&ma_generic_termios);
  }
}

/* Initialize.
 */
 
static int ma_generic_init(int argc,char **argv) {

  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    if (!strcmp(arg,"--help")) {
      ma_generic_print_usage(argv[0]);
      return -1;
      
    } else if (!strcmp(arg,"--no-signals")) {
      ma_generic_use_signals=0;
      
    } else if (!strcmp(arg,"--no-stdin")) {
      ma_generic_use_stdin=0;
      
    } else if (!strcmp(arg,"--no-stdout")) {
      ma_generic_use_stdout=0;
      
    } else if (!memcmp(arg,"--keyhold=",10)) {
      ma_generic_keyhold_time=0;
      int p=10; for (;arg[p];p++) {
        if ((arg[p]<'0')||(arg[p]>'9')||(ma_generic_keyhold_time>0x10000)) {
          fprintf(stderr,"%s: Decimal integer required for 'keyhold'\n",argv[0]);
          return 1;
        }
        ma_generic_keyhold_time*=10;
        ma_generic_keyhold_time+=arg[p]-'0';
      }
      
    } else if (!memcmp(arg,"--files=",8)) {
      ma_generic_file_sandbox=arg+8;
      
    } else {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],arg);
      return -1;
    }
  }

  if (ma_generic_use_stdin) {
    if (tcgetattr(STDIN_FILENO,&ma_generic_termios)>=0) {
      struct termios termios=ma_generic_termios;
      termios.c_lflag&=~(ICANON|ECHO);
      if (tcsetattr(STDIN_FILENO,TCSANOW,&termios)>=0) {
        ma_generic_gotstdin=1;
      }
    }
  }
  
  if (ma_generic_use_signals) {
    signal(SIGINT,ma_generic_rcvsig);
  }
  
  setup();
  ma_generic_start_time=ma_generic_now();
  return 0;
}

/* Update.
 * Returns >0 to proceed.
 */
 
static int ma_generic_update() {
  ma_generic_framec++;
  loop();
  if (ma_generic_quit_requested) return 0;
  return 1;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if (ma_generic_init(argc,argv)<0) {
    ma_generic_quit();
    return 1;
  }
  while (!ma_generic_sigc) {
    int err=ma_generic_update();
    if (err<0) {
      ma_generic_quit();
      return 1;
    }
    if (!err) break;
  }
  ma_generic_quit();
  return 0;
}
