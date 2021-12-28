#include "ps_macioc_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct ps_macioc ps_macioc={0};
int ma_macioc_micros_per_frame=0;

/* Mangle path.
 */
 
int ma_macos_mangle_path(
  const char *clientpath,
  int (*cb)(const char *path,void *userdata),
  void *userdata
) {

  // Measure path and remove "/Glacer/" prefix if present.
  if (!clientpath||!clientpath[0]) return 0;
  int clientpathc=0; while (clientpath[clientpathc]) clientpathc++;
  if ((clientpathc>=9)&&!memcmp(clientpath,"/Glacier/",8)) {
    clientpath+=8;
    clientpathc-=8;
  }

  char path[1024];
  int pathc,err;

  // Try the mutable path: ~/Library/Preferences/com.aksommerville.glacierracer/*
  // On second thought, do this only for the one known mutable file.
  // If we ever have a mutable config file (eg input map), that would go here too.
  if ((clientpathc==7)&&!memcmp(clientpath,"hiscore",7)) {
    pathc=0;
    const char *home=getenv("HOME");
    if (home&&home[0]) {
      int homec=0; while (home[homec]) homec++;
      if (pathc<=sizeof(path)-homec) {
        memcpy(path+pathc,home,homec);
        pathc+=homec;
      }
    } else {
      const char *user=getenv("USER");
      if (user&&user[0]) {
        int userc=0; while (user[userc]) userc++;
        if (pathc+7+userc<=sizeof(path)) {
          memcpy(path+pathc,"/Users/",7);
          pathc+=7;
          memcpy(path+pathc,user,userc);
          pathc+=userc;
        }
      }
    }
    if (pathc) {
      const char *mid="/Library/Preferences/com.aksommerville.glacierracer/";
      int midc=0; while (mid[midc]) midc++;
      if (pathc<sizeof(path)-midc-clientpathc) {
        memcpy(path+pathc,mid,midc);
        pathc+=midc;
        memcpy(path+pathc,clientpath,clientpathc);
        pathc+=clientpathc;
        path[pathc]=0;
        if (err=cb(path,userdata)) return err;
      }
    }
  }

  // Try under the bundle's Resources.
  const char *respath=[[[NSBundle mainBundle] resourcePath] UTF8String];
  if (respath&&respath[0]) {
    int respathc=0; while (respath[respathc]) respathc++;
    const char *mid="/data/";
    int midc=6;
    if (respathc+midc+clientpathc<sizeof(path)) {
      memcpy(path,respath,respathc);
      memcpy(path+respathc,mid,midc);
      memcpy(path+respathc+midc,clientpath,clientpathc);
      path[respathc+midc+clientpathc]=0;
      if (err=cb(path,userdata)) return err;
    }
  }

  // We could try other things but whatever.
  return 0;
}

/* Reopen TTY, because normal Mac apps launch without one.
 */

static void ma_macioc_reopen_tty(const char *path) {
  int fd=open(path,O_RDWR);
  if (fd<0) return;
  dup2(fd,STDIN_FILENO);
  dup2(fd,STDOUT_FILENO);
  dup2(fd,STDERR_FILENO);
  close(fd);
}

/* First pass through argv.
 */

static int ps_macioc_argv_prerun(int argc,char **argv) {
  int argp;
  for (argp=1;argp<argc;argp++) {
    const char *k=argv[argp];
    if (!k) continue;
    if ((k[0]!='-')||(k[1]!='-')||!k[2]) continue;
    k+=2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=k+kc;
    int vc=0;
    if (v[0]=='=') {
      v++;
      while (v[vc]) vc++;
    }

    if ((kc==10)&&!memcmp(k,"reopen-tty",10)) {
      ma_macioc_reopen_tty(v);
      argv[argp]="";

    } else if ((kc==5)&&!memcmp(k,"chdir",5)) {
      if (chdir(v)<0) return -1;
      argv[argp]="";

    }
  }
  return 0;
}

/* Configure.
 */

static int ps_macioc_configure(int argc,char **argv) {
  if (ps_macioc_argv_prerun(argc,argv)<0) return -1;
  return 0;
}

/* Main.
 */

int ma_macioc_main(int argc,char **argv,const struct ma_macioc_delegate *delegate) {

  if (ps_macioc.init) return 1;
  memset(&ps_macioc,0,sizeof(struct ps_macioc));
  ps_macioc.init=1;
  
  if (delegate) memcpy(&ps_macioc.delegate,delegate,sizeof(struct ma_macioc_delegate));

  if (ps_macioc_configure(argc,argv)<0) return 1;

  return NSApplicationMain(argc,(const char**)argv);
}

/* Simple termination.
 */
 
void ma_macioc_quit() {
  [NSApplication.sharedApplication terminate:nil];
  fprintf(stderr,"!!! [NSApplication.sharedApplication terminate:nil] did not terminate execution. Using exit() instead !!!\n");
  exit(1);
}

/* Abort.
 */
 
void ma_macioc_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    fprintf(stderr,"%.*s\n",msgc,msg);
  }
  ma_macioc_quit();
}

/* Callback triggers.
 */
 
int ps_macioc_call_init() {
  int result=(ps_macioc.delegate.init?ps_macioc.delegate.init():0);
  ps_macioc.delegate.init=0; // Guarantee only one call.
  if (result<0) return -1;
  if (!ma_macioc_micros_per_frame) return -1;
  return 0;
}

void ps_macioc_call_quit() {
  ps_macioc.terminate=1;
  if (ps_macioc.delegate.quit) {
    ps_macioc.delegate.quit();
    ps_macioc.delegate.quit=0; // Guarantee only one call.
  }
}

@implementation AKAppDelegate

/* Main loop.
 * This runs on a separate thread.
 */

-(void)mainLoop:(id)ignore {

  uint32_t next_time=micros();
  
  while (1) {

    if (ps_macioc.terminate) break;

    uint32_t now=micros();
    while (now<next_time) {
      usleep(next_time-now);
      now=micros();
    }
    next_time+=ma_macioc_micros_per_frame;

    if (ps_macioc.terminate) break;

    if (ps_macioc.update_in_progress) {
      //ps_log(MACIOC,TRACE,"Dropping frame due to update still running.");
      continue;
    }

    /* With 'waitUntilDone:0', we will always be on manual timing.
     * I think that's OK. And window resizing is much more responsive this way.
     * Update:
     *   !!! After upgrading from 10.11 to 10.13, all the timing got fucked.
     *   Switching to 'waitUntilDone:1' seems to fix it.
     *   If the only problem that way in 10.11 was choppy window resizing, so be it.
     *   Resize seems OK with '1' and OS 10.13.
     */
    [self performSelectorOnMainThread:@selector(updateMain:) withObject:nil waitUntilDone:1];
  
  }
}

/* Route call from main loop.
 * This runs on the main thread.
 */

-(void)updateMain:(id)ignore {
  ps_macioc.update_in_progress=1;
  if (ps_macioc.delegate.update) {
    int err=ps_macioc.delegate.update();
    if (err<0) {
      ma_macioc_abort("Error %d updating application.",err);
    }
  }
  ps_macioc.update_in_progress=0;
}

/* Finish launching.
 * We fire the 'init' callback and launch an updater thread.
 */

-(void)applicationDidFinishLaunching:(NSNotification*)notification {
  
  int err=ps_macioc_call_init();
  if (err<0) {
    ma_macioc_abort("Initialization failed (%d). Aborting.",err);
  }

  [NSThread detachNewThreadSelector:@selector(mainLoop:) toTarget:self withObject:nil];
  
}

/* Termination.
 */

-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  return NSTerminateNow;
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return 1;
}

-(void)applicationWillTerminate:(NSNotification*)notification {
  ps_macioc_call_quit();
}

/* Receive system error.
 */

-(NSError*)application:(NSApplication*)application willPresentError:(NSError*)error {
  const char *message=error.localizedDescription.UTF8String;
  fprintf(stderr,"%s\n",message);
  return error;
}

/* Change input focus.
 */

-(void)applicationDidBecomeActive:(NSNotification*)notification {
  if (ps_macioc.focus) return;
  ps_macioc.focus=1;
}

-(void)applicationDidResignActive:(NSNotification*)notification {
  if (!ps_macioc.focus) return;
  ps_macioc.focus=0;
}

@end
