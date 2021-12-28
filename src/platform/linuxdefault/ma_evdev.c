#include "ma_ld_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/poll.h>
#include <sys/inotify.h>
#include <linux/input.h>

// Must end with slash.
#define MA_EVDEV_DIR "/dev/input/"

#define MA_EVDEV_AXIS_MODE_BUTTON 1 /* >=hi ON, <hi OFF */
#define MA_EVDEV_AXIS_MODE_TWOWAY 2 /* <=lo left/up, >=hi right/down */
#define MA_EVDEV_AXIS_MODE_HAT    3 /* (v-lo) = (0,1,2,3,4,5,6,7) = (N,NE,E,SE,S,SW,W,NW)... why do these exist */

#define MA_EVDEV_AXIS_HORZ 1
#define MA_EVDEV_AXIS_VERT 2

/* Instance definition.
 */
 
struct ma_evdev {
  int (*cb_button)(struct ma_evdev *evdev,uint16_t btnid,int value);
  void *userdata;
  int infd;
  int rescan;
  struct ma_evdev_device {
    int fd;
    int evno;
    uint16_t state;
    struct ma_evdev_axis {
      uint16_t code;
      int mode;
      int lo,hi;
      int axis;
      int v;
    } *axisv;
    int axisc,axisa;
  } *devicev;
  int devicec,devicea;
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
};

/* Cleanup.
 */
 
static void ma_evdev_device_cleanup(struct ma_evdev_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->axisv) free(device->axisv);
}
 
void ma_evdev_del(struct ma_evdev *evdev) {
  if (!evdev) return;
  
  if (evdev->infd>=0) close(evdev->infd);
  if (evdev->pollfdv) free(evdev->pollfdv);
  
  if (evdev->devicev) {
    while (evdev->devicec-->0) ma_evdev_device_cleanup(evdev->devicev+evdev->devicec);
    free(evdev->devicev);
  }
  
  free(evdev);
}

/* Initialize inotify.
 */
 
static int ma_evdev_init_inotify(struct ma_evdev *evdev) {
  if ((evdev->infd=inotify_init())<0) return -1;
  if (inotify_add_watch(evdev->infd,MA_EVDEV_DIR,IN_CREATE|IN_ATTRIB)<0) {
    close(evdev->infd);
    evdev->infd=-1;
    return -1;
  }
  return 0;
}

/* New.
 */
 
struct ma_evdev *ma_evdev_new(
  int (*cb_button)(struct ma_evdev *evdev,uint16_t btnid,int value),
  void *userdata
) {
  struct ma_evdev *evdev=calloc(1,sizeof(struct ma_evdev));
  if (!evdev) return 0;
  
  evdev->cb_button=cb_button;
  evdev->userdata=userdata;
  
  if (ma_evdev_init_inotify(evdev)<0) {
    fprintf(stderr,"%s: Failed to initialize inotify. Joystick connections will not be detected.\n",MA_EVDEV_DIR);
  }
  evdev->rescan=1;
  
  return evdev;
}

/* Trivial accessors.
 */
 
void *ma_evdev_get_userdata(const struct ma_evdev *evdev) {
  return evdev->userdata;
}

/* Find connected device.
 */
 
static struct ma_evdev_device *ma_evdev_get_device_by_evno(const struct ma_evdev *evdev,int evno) {
  int i=evdev->devicec;
  struct ma_evdev_device *device=evdev->devicev;
  for (;i-->0;device++) if (device->evno==evno) return device;
  return 0;
}
 
static struct ma_evdev_device *ma_evdev_get_device_by_fd(const struct ma_evdev *evdev,int fd) {
  int i=evdev->devicec;
  struct ma_evdev_device *device=evdev->devicev;
  for (;i-->0;device++) if (device->fd==fd) return device;
  return 0;
}

/* Add device.
 */
 
static struct ma_evdev_device *ma_evdev_add_device(struct ma_evdev *evdev,int evno,int fd) {
  if (fd<0) return 0;
  
  if (evdev->devicec>=evdev->devicea) {
    int na=evdev->devicea+8;
    if (na>INT_MAX/sizeof(struct ma_evdev_device)) return 0;
    void *nv=realloc(evdev->devicev,sizeof(struct ma_evdev_device)*na);
    if (!nv) return 0;
    evdev->devicev=nv;
    evdev->devicea=na;
  }
  
  struct ma_evdev_device *device=evdev->devicev+evdev->devicec++;
  memset(device,0,sizeof(struct ma_evdev_device));
  device->evno=evno;
  device->fd=fd;
  return device;
}

/* Add axis to device, or ignore it.
 */
 
static int ma_evdev_device_add_axis(
  struct ma_evdev *evdev,
  struct ma_evdev_device *device,
  uint16_t code,
  const struct input_absinfo *absinfo
) {
  
  if (device->axisc>=device->axisa) {
    int na=device->axisa+8;
    if (na>INT_MAX/sizeof(struct ma_evdev_axis)) return -1;
    void *nv=realloc(device->axisv,sizeof(struct ma_evdev_axis)*na);
    if (!nv) return -1;
    device->axisv=nv;
    device->axisa=na;
  }
  struct ma_evdev_axis *axis=device->axisv+device->axisc;
  
  // Resting value between limits exclusive? Call it two-way.
  if ((absinfo->value>absinfo->minimum)&&(absinfo->value<absinfo->maximum)) {
    memset(axis,0,sizeof(struct ma_evdev_axis));
    axis->code=code;
    axis->mode=MA_EVDEV_AXIS_MODE_TWOWAY;
    int mid=(absinfo->minimum+absinfo->maximum)>>1;
    axis->lo=(absinfo->minimum+mid)>>1;
    axis->hi=(absinfo->maximum+mid)>>1;
    if (axis->lo>=mid) axis->lo=mid-1;
    if (axis->hi<=mid) axis->hi=mid+1;
    // Mostly Linux's X axes have even codes... pick off the exceptions.
    switch (code) {
      case ABS_RX: axis->axis=MA_EVDEV_AXIS_HORZ; break;
      case ABS_RY: axis->axis=MA_EVDEV_AXIS_VERT; break;
      default: if (code&1) axis->axis=MA_EVDEV_AXIS_VERT; else axis->axis=MA_EVDEV_AXIS_HORZ;
    }
    device->axisc++;
    return 0;
  }
  
  // At least two values, and resting at the very bottom? Call it button.
  if ((absinfo->maximum>=absinfo->minimum+1)&&(absinfo->value==absinfo->minimum)) {
    memset(axis,0,sizeof(struct ma_evdev_axis));
    axis->code=code;
    axis->mode=MA_EVDEV_AXIS_MODE_BUTTON;
    axis->hi=(absinfo->minimum+absinfo->maximum)>>1;
    axis->axis=(code&1)?MA_BUTTON_A:MA_BUTTON_B;
    device->axisc++;
    return 0;
  }
  
  // Exactly 8 values? Call it hat.
  if (absinfo->maximum==absinfo->minimum+7) {
    memset(axis,0,sizeof(struct ma_evdev_axis));
    axis->code=code;
    axis->mode=MA_EVDEV_AXIS_MODE_HAT;
    axis->lo=absinfo->minimum;
    axis->v=-1;
    device->axisc++;
    return 0;
  }
  
  // Something else, ignore it.
  return 0;
}

/* Try to connect to one file, no harm if we can't open it or whatever.
 */
 
static int ma_evdev_try_file(struct ma_evdev *evdev,const char *base,int basec) {
  if (basec<0) { basec=0; while (base[basec]) basec++; }
  
  if ((basec<6)||memcmp(base,"event",5)) return 0;
  int evno=0,p=5;
  for (;p<basec;p++) {
    int digit=base[p]-'0';
    if ((digit<0)||(digit>9)) return 0;
    evno*=10;
    evno+=digit;
  }
  if (ma_evdev_get_device_by_evno(evdev,evno)) return 0;
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%.*s",(int)sizeof(MA_EVDEV_DIR)-1,MA_EVDEV_DIR,basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  int version=0;
  if (ioctl(fd,EVIOCGVERSION,&version)<0) {
    close(fd);
    return 0;
  }
  
  int grab=1;
  ioctl(fd,EVIOCGRAB,grab);
  
  struct ma_evdev_device *device=ma_evdev_add_device(evdev,evno,fd);
  if (!device) {
    close(fd);
    return -1;
  }
  
  char name[64];
  int namec=ioctl(fd,EVIOCGNAME(sizeof(name)),name);
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  
  uint8_t absbit[(ABS_CNT+7)>>3]={0};
  ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(absbit)),absbit);
  int major=0; for (;major<sizeof(absbit);major++) {
    if (!absbit[major]) continue;
    int minor=0; for (;minor<8;minor++) {
      if (!(absbit[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      
      struct input_absinfo absinfo={0};
      if (ioctl(fd,EVIOCGABS(code),&absinfo)<0) continue;
      
      ma_evdev_device_add_axis(evdev,device,code,&absinfo);
    }
  }
  
  fprintf(stderr,"Connected input device: %.*s\n",namec,name);
  return 1;
}

/* Scan.
 */
 
static int ma_evdev_scan(struct ma_evdev *evdev) {
  DIR *dir=opendir(MA_EVDEV_DIR);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (ma_evdev_try_file(evdev,de->d_name,-1)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Close any file (inotify or device).
 */
 
static int ma_evdev_drop_fd(struct ma_evdev *evdev,int fd) {
  if (fd==evdev->infd) {
    fprintf(stderr,"%s: inotify shut down. Joystick connections will not be detected.\n",MA_EVDEV_DIR);
    close(evdev->infd);
    evdev->infd=-1;
    return 0;
  }
  struct ma_evdev_device *device=evdev->devicev;
  int i=0;
  for (;i<evdev->devicec;i++,device++) {
    if (device->fd==fd) {
      int err=0;
      if (device->state) {
        uint16_t mask=0x8000;
        for (;mask;mask>>=1) {
          if (device->state&mask) {
            if (evdev->cb_button(evdev,mask,0)<0) err=-1;
          }
        }
      }
      ma_evdev_device_cleanup(device);
      evdev->devicec--;
      memmove(device,device+1,sizeof(struct ma_evdev_device)*(evdev->devicec-i));
      return err;
    }
  }
  return 0;
}

/* Read inotify.
 */
 
static int ma_evdev_read_inotify(struct ma_evdev *evdev) {
  char buf[1024];
  int bufc=read(evdev->infd,buf,sizeof(buf));
  if (bufc<=0) return ma_evdev_drop_fd(evdev,evdev->infd);
  int bufp=0;
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    int basec=0;
    while ((basec<event->len)&&event->name[basec]) basec++;
    ma_evdev_try_file(evdev,event->name,basec);
  }
  return 0;
}

/* Find axis.
 */
 
static struct ma_evdev_axis *ma_evdev_device_find_axis(
  const struct ma_evdev_device *device,
  uint16_t code
) {
  int lo=0,hi=device->axisc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct ma_evdev_axis *axis=device->axisv+ck;
         if (code<axis->code) hi=ck;
    else if (code>axis->code) lo=ck+1;
    else return axis;
  }
  return 0;
}

/* Read device.
 */
 
static int ma_evdev_key(struct ma_evdev *evdev,struct ma_evdev_device *device,uint16_t btnid,int value) {
  if (value) {
    if (device->state&btnid) return 0;
    device->state|=btnid;
    value=1;
  } else {
    if (!(device->state&btnid)) return 0;
    device->state&=~btnid;
  }
  return evdev->cb_button(evdev,btnid,value);
}
 
static int ma_evdev_event(
  struct ma_evdev *evdev,
  struct ma_evdev_device *device,
  const struct input_event *event
) {
  switch (event->type) {
  
    case EV_ABS: {
        struct ma_evdev_axis *axis=ma_evdev_device_find_axis(device,event->code);
        if (axis) switch (axis->mode) {
        
          case MA_EVDEV_AXIS_MODE_BUTTON: {
              int v=(event->value>=axis->hi)?1:0;
              if (v!=axis->v) {
                axis->v=v;
                if (v) device->state|=axis->axis;
                else device->state&=~axis->axis;
                return evdev->cb_button(evdev,axis->axis,v);
              }
            } break;
            
          case MA_EVDEV_AXIS_MODE_TWOWAY: {
              int v=(event->value<=axis->lo)?-1:(event->value>=axis->hi)?1:0;
              if (v!=axis->v) {
                axis->v=v;
                if (axis->axis==MA_EVDEV_AXIS_HORZ) {
                  if ((v<=0)&&(device->state&MA_BUTTON_RIGHT)) {
                    device->state&=~MA_BUTTON_RIGHT;
                    if (evdev->cb_button(evdev,MA_BUTTON_RIGHT,0)<0) return -1;
                  }
                  if ((v>=0)&&(device->state&MA_BUTTON_LEFT)) {
                    device->state&=~MA_BUTTON_LEFT;
                    if (evdev->cb_button(evdev,MA_BUTTON_LEFT,0)<0) return -1;
                  }
                  if ((v<0)&&!(device->state&MA_BUTTON_LEFT)) {
                    device->state|=MA_BUTTON_LEFT;
                    if (evdev->cb_button(evdev,MA_BUTTON_LEFT,1)<0) return -1;
                  }
                  if ((v>0)&&!(device->state&MA_BUTTON_RIGHT)) {
                    device->state|=MA_BUTTON_RIGHT;
                    if (evdev->cb_button(evdev,MA_BUTTON_RIGHT,1)<0) return -1;
                  }
                } else {
                  if ((v<=0)&&(device->state&MA_BUTTON_DOWN)) {
                    device->state&=~MA_BUTTON_DOWN;
                    if (evdev->cb_button(evdev,MA_BUTTON_DOWN,0)<0) return -1;
                  }
                  if ((v>=0)&&(device->state&MA_BUTTON_UP)) {
                    device->state&=~MA_BUTTON_UP;
                    if (evdev->cb_button(evdev,MA_BUTTON_UP,0)<0) return -1;
                  }
                  if ((v<0)&&!(device->state&MA_BUTTON_UP)) {
                    device->state|=MA_BUTTON_UP;
                    if (evdev->cb_button(evdev,MA_BUTTON_UP,1)<0) return -1;
                  }
                  if ((v>0)&&!(device->state&MA_BUTTON_DOWN)) {
                    device->state|=MA_BUTTON_DOWN;
                    if (evdev->cb_button(evdev,MA_BUTTON_DOWN,1)<0) return -1;
                  }
                }
              }
            } break;
            
          case MA_EVDEV_AXIS_MODE_HAT: {
              int v=event->value-axis->lo;
              if ((v<0)||(v>=8)) v=-1;
              if (v!=axis->v) {
                uint16_t nstate=0;
                switch (axis->v=v) {
                  case 0: nstate=MA_BUTTON_UP; break;
                  case 1: nstate=MA_BUTTON_UP|MA_BUTTON_RIGHT; break;
                  case 2: nstate=MA_BUTTON_RIGHT; break;
                  case 3: nstate=MA_BUTTON_RIGHT|MA_BUTTON_DOWN; break;
                  case 4: nstate=MA_BUTTON_DOWN; break;
                  case 5: nstate=MA_BUTTON_DOWN|MA_BUTTON_LEFT; break;
                  case 6: nstate=MA_BUTTON_LEFT; break;
                  case 7: nstate=MA_BUTTON_LEFT|MA_BUTTON_UP; break;
                }
                uint16_t ostate=device->state&(MA_BUTTON_UP|MA_BUTTON_DOWN|MA_BUTTON_LEFT|MA_BUTTON_RIGHT);
                device->state=(device->state&(MA_BUTTON_A|MA_BUTTON_B))|nstate;
                #define _(tag) \
                  if ((nstate&MA_BUTTON_##tag)&&!(ostate&MA_BUTTON_##tag)) { \
                    if (evdev->cb_button(evdev,MA_BUTTON_##tag,1)<0) return -1; \
                  } else if (!(nstate&MA_BUTTON_##tag)&&(ostate&MA_BUTTON_##tag)) { \
                    if (evdev->cb_button(evdev,MA_BUTTON_##tag,0)<0) return -1; \
                  }
                _(UP)
                _(DOWN)
                _(LEFT)
                _(RIGHT)
                #undef _
              }
            } break;
        }
      } break;
      
    case EV_KEY: switch (event->code) {
        case KEY_ESC: ma_ld_quit_requested=1; return 0;
        case KEY_UP: return ma_evdev_key(evdev,device,MA_BUTTON_UP,event->value);
        case KEY_DOWN: return ma_evdev_key(evdev,device,MA_BUTTON_DOWN,event->value);
        case KEY_LEFT: return ma_evdev_key(evdev,device,MA_BUTTON_LEFT,event->value);
        case KEY_RIGHT: return ma_evdev_key(evdev,device,MA_BUTTON_RIGHT,event->value);
        case KEY_Z: return ma_evdev_key(evdev,device,MA_BUTTON_A,event->value);
        case KEY_X: return ma_evdev_key(evdev,device,MA_BUTTON_B,event->value);
        default: {
            if (event->code<0x100) return 0;
            return ma_evdev_key(evdev,device,(event->code&1)?MA_BUTTON_B:MA_BUTTON_A,event->value);
          }
      } break;
      
  }
  return 0;
}
 
static int ma_evdev_read_device(struct ma_evdev *evdev,struct ma_evdev_device *device) {
  if (!device) return -1;
  struct input_event eventv[32];
  int eventc=read(device->fd,eventv,sizeof(eventv));
  if (eventc<=0) return ma_evdev_drop_fd(evdev,device->fd);
  eventc/=sizeof(struct input_event);
  const struct input_event *event=eventv;
  for (;eventc-->0;event++) {
    if (ma_evdev_event(evdev,device,event)<0) return -1;
  }
  return 0;
}

/* Add to poll list.
 */
 
static int ma_evdev_pollfdv_add(struct ma_evdev *evdev,int fd) {
  
  if (evdev->pollfdc>=evdev->pollfda) {
    int na=evdev->pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(evdev->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    evdev->pollfdv=nv;
    evdev->pollfda=na;
  }
  
  struct pollfd *pollfd=evdev->pollfdv+evdev->pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}

/* Update.
 */
 
int ma_evdev_update(struct ma_evdev *evdev) {
  if (evdev->rescan) {
    ma_evdev_scan(evdev);
    evdev->rescan=0;
  }
  
  evdev->pollfdc=0;
  if (evdev->infd>=0) ma_evdev_pollfdv_add(evdev,evdev->infd);
  struct ma_evdev_device *device=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;device++) ma_evdev_pollfdv_add(evdev,device->fd);
  if (evdev->pollfdc<1) return 0;
  
  if (poll(evdev->pollfdv,evdev->pollfdc,0)<1) return 0;
  
  struct pollfd *pollfd=evdev->pollfdv;
  for (i=evdev->pollfdc;i-->0;pollfd++) {
    if (pollfd->revents&(POLLERR|POLLHUP)) {
      ma_evdev_drop_fd(evdev,pollfd->fd);
    } else if (pollfd->revents&POLLIN) {
      if (pollfd->fd==evdev->infd) {
        ma_evdev_read_inotify(evdev);
      } else {
        ma_evdev_read_device(evdev,ma_evdev_get_device_by_fd(evdev,pollfd->fd));
      }
    }
  }
  
  return 0;
}
