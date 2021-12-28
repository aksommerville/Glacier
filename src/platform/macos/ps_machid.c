#include "ps_machid.h"
#include <stdio.h>
#include <IOKit/hid/IOHIDLib.h>

/* Private definitions.
 */

#define AKMACHID_RUNLOOP_MODE CFSTR("com.aksommerville.ps_machid")

struct ps_machid_btn {
  int btnid;
  int usage;
  int value;
  int lo,hi;
  int btnid_aux; // If nonzero, this is a hat switch to convert from polar to cartesian.
};

struct ps_machid_dev {
  IOHIDDeviceRef obj; // weak
  int devid;
  int vid,pid;
  char *mfrname,*prodname,*serial;
  int usagepage,usage;
  struct ps_machid_btn *btnv; int btnc,btna;
};

/* Globals.
 */

static struct {
  IOHIDManagerRef hidmgr;
  struct ps_machid_dev *devv; // Sorted by (obj)
  int devc,deva;
  struct ps_machid_delegate delegate; // All callbacks are set, possibly with defaults.
  int error; // Sticky error from callbacks, reported at end of update.
} ps_machid={0};

/* Get integer field from device.
 */

static int dev_get_int(IOHIDDeviceRef dev,CFStringRef k) {
  CFNumberRef vobj=IOHIDDeviceGetProperty(dev,k);
  if (!vobj) return 0;
  if (CFGetTypeID(vobj)!=CFNumberGetTypeID()) return 0;
  int v=0;
  CFNumberGetValue(vobj,kCFNumberIntType,&v);
  return v;
}

/* Cleanup device record.
 */

static void ps_machid_dev_cleanup(struct ps_machid_dev *dev) {
  if (!dev) return;
  if (dev->btnv) free(dev->btnv);
  if (dev->mfrname) free(dev->mfrname);
  if (dev->prodname) free(dev->prodname);
  if (dev->serial) free(dev->serial);
  memset(dev,0,sizeof(struct ps_machid_dev));
}

/* Search global device list.
 */

static int ps_machid_dev_search_devid(int devid) {
  int i; for (i=0;i<ps_machid.devc;i++) if (ps_machid.devv[i].devid==devid) return i;
  return -ps_machid.devc-1;
}

static int ps_machid_dev_search_obj(IOHIDDeviceRef obj) {
  int lo=0,hi=ps_machid.devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (obj<ps_machid.devv[ck].obj) hi=ck;
    else if (obj>ps_machid.devv[ck].obj) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int ps_machid_devid_unused() {
  if (ps_machid.devc<1) return 1;
  
  /* Most cases: return one more than the highest used ID. */
  int i,top=0;
  for (i=0;i<ps_machid.devc;i++) {
    if (ps_machid.devv[i].devid>top) top=ps_machid.devv[i].devid;
  }
  if (top<INT_MAX) return top+1;

  /* If we've reached ID INT_MAX, there must be a gap somewhere. Find it. */
  int devid=1;
  for (i=0;i<ps_machid.devc;i++) if (ps_machid.devv[i].devid==devid) { devid++; i=-1; }
  return devid;
}

/* Insert device to global list.
 */

static int ps_machid_dev_insert_validate(int p,IOHIDDeviceRef obj,int devid) {
  if ((p<0)||(p>ps_machid.devc)) return -1;
  if (!obj||(CFGetTypeID(obj)!=IOHIDDeviceGetTypeID())) return -1;
  if ((devid<1)||(devid>0xffff)) return -1;
  if (p&&(obj<=ps_machid.devv[p-1].obj)) return -1;
  if ((p<ps_machid.devc)&&(obj>=ps_machid.devv[p].obj)) return -1;
  int i; for (i=0;i<ps_machid.devc;i++) if (ps_machid.devv[i].devid==devid) return -1;
  return 0;
}

static int ps_machid_devv_require() {
  if (ps_machid.devc<ps_machid.deva) return 0;
  int na=ps_machid.deva+8;
  if (na>INT_MAX/sizeof(struct ps_machid_dev)) return -1;
  void *nv=realloc(ps_machid.devv,sizeof(struct ps_machid_dev)*na);
  if (!nv) return -1;
  ps_machid.devv=nv;
  ps_machid.deva=na;
  return 0;
}

static int ps_machid_dev_insert(int p,IOHIDDeviceRef obj,int devid) {

  if (ps_machid_dev_insert_validate(p,obj,devid)<0) return -1;
  if (ps_machid_devv_require()<0) return -1;
  
  struct ps_machid_dev *dev=ps_machid.devv+p;
  memmove(dev+1,dev,sizeof(struct ps_machid_dev)*(ps_machid.devc-p));
  ps_machid.devc++;
  memset(dev,0,sizeof(struct ps_machid_dev));
  dev->obj=obj;
  dev->devid=devid;
  
  return 0;
}

/* Remove from global device list.
 */

static int ps_machid_dev_remove(int p) {
  if ((p<0)||(p>=ps_machid.devc)) return -1;
  ps_machid_dev_cleanup(ps_machid.devv+p);
  ps_machid.devc--;
  memmove(ps_machid.devv+p,ps_machid.devv+p+1,sizeof(struct ps_machid_dev)*(ps_machid.devc-p));
  return 0;
}

/* Search buttons in device.
 */

static int ps_machid_dev_search_button(struct ps_machid_dev *dev,int btnid) {
  if (!dev) return -1;
  int lo=0,hi=dev->btnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (btnid<dev->btnv[ck].btnid) hi=ck;
    else if (btnid>dev->btnv[ck].btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add button record to device.
 */

static int ps_machid_dev_insert_button(struct ps_machid_dev *dev,int p,int btnid,int usage,int lo,int hi,int v) {
  if (!dev||(p<0)||(p>dev->btnc)) return -1;
  
  if (dev->btnc>=dev->btna) {
    int na=dev->btna+8;
    if (na>INT_MAX/sizeof(struct ps_machid_btn)) return -1;
    void *nv=realloc(dev->btnv,sizeof(struct ps_machid_btn)*na);
    if (!nv) return -1;
    dev->btnv=nv;
    dev->btna=na;
  }

  struct ps_machid_btn *btn=dev->btnv+p;
  memmove(btn+1,btn,sizeof(struct ps_machid_btn)*(dev->btnc-p));
  dev->btnc++;
  memset(btn,0,sizeof(struct ps_machid_btn));
  btn->btnid=btnid;
  btn->usage=usage;
  btn->lo=lo;
  btn->hi=hi;
  if ((v>=lo)&&(v<=hi)) btn->value=v;
  else if ((lo<=0)&&(hi>=0)) btn->value=0;
  else if (lo>0) btn->value=lo;
  else btn->value=hi;

  /* Is this a hat switch? Our logic here is not perfect. I'm testing against my Zelda joystick. */
  if ((usage==0x00010039)&&(lo==0)&&(hi==7)) {
    btn->btnid_aux=1; // Just make it nonzero, and we will assign final IDs after reading all real buttons.
    btn->lo=INT_MIN; // Hats report a value outside the declared range for "none", which is annoying.
    btn->hi=INT_MAX; // Annoying but well within spec (see USB-HID HUT)
    btn->value=8; // Start out of range, ie (0,0)
  }

  return 0;
}

static int ps_machid_dev_define_button(struct ps_machid_dev *dev,int btnid,int usage,int lo,int hi,int v) {
  int p=ps_machid_dev_search_button(dev,btnid);
  if (p>=0) return 0; // Already have this btnid, don't panic. Keep the first definition.
  return ps_machid_dev_insert_button(dev,-p-1,btnid,usage,lo,hi,v);
}

/* Look for any nonzero (btnid_aux) and give it a positive unused ID.
 */

static int ps_machid_unused_btnid(const struct ps_machid_dev *dev) {
  int highest=0,i=dev->btnc;
  const struct ps_machid_btn *btn=dev->btnv;
  for (;i-->0;btn++) {
    if (btn->btnid>=highest) highest=btn->btnid;
    if (btn->btnid_aux>=highest) highest=btn->btnid_aux;
  }
  if (highest==INT_MAX) return -1;
  return highest+1;
}

static int ps_machid_dev_assign_aux_btnid(struct ps_machid_dev *dev) {
  struct ps_machid_btn *btn=dev->btnv;
  int i=dev->btnc; for (;i-->0;btn++) {
    if (!btn->btnid_aux) continue;
    if ((btn->btnid_aux=ps_machid_unused_btnid(dev))<0) {
      // This is extremely improbable, but don't fail entirely if it happens.
      btn->btnid_aux=0;
    }
  }
  return 0;
}

/* Welcome a new device, before exposing to user.
 */

static char *ps_machid_dev_get_string(struct ps_machid_dev *dev,CFStringRef key) {
  char buf[256]={0};

  CFStringRef string=IOHIDDeviceGetProperty(dev->obj,key);
  if (!string) return 0;
  if (!CFStringGetCString(string,buf,sizeof(buf),kCFStringEncodingUTF8)) return 0;
  int bufc=0; while ((bufc<sizeof(buf))&&buf[bufc]) bufc++;
  if (!bufc) return 0;

  /* Force non-G0 to space, then trim leading and trailing spaces. */
  int i; for (i=0;i<bufc;i++) {
    if ((buf[i]<0x20)||(buf[i]>0x7e)) buf[i]=0x20;
  }
  while (bufc&&(buf[bufc-1]==0x20)) bufc--;
  int leadc=0; while ((leadc<bufc)&&(buf[leadc]==0x20)) leadc++;
  if (leadc==bufc) return 0;
  
  char *dst=malloc(bufc-leadc+1);
  if (!dst) return 0;
  memcpy(dst,buf+leadc,bufc-leadc);
  dst[bufc-leadc]=0;
  return dst;
}

static int ps_machid_dev_apply_IOHIDElement(struct ps_machid_dev *dev,IOHIDElementRef element) {
      
  IOHIDElementCookie cookie=IOHIDElementGetCookie(element);
  CFIndex lo=IOHIDElementGetLogicalMin(element);
  CFIndex hi=IOHIDElementGetLogicalMax(element);
  if ((int)cookie<INT_MIN) cookie=INT_MIN; else if (cookie>INT_MAX) cookie=INT_MAX;
  if ((int)lo<INT_MIN) lo=INT_MIN; else if (lo>INT_MAX) lo=INT_MAX;
  if ((int)hi<INT_MIN) hi=INT_MIN; else if (hi>INT_MAX) hi=INT_MAX;
  if (lo>hi) { lo=INT_MIN; hi=INT_MAX; }
  uint16_t usagepage=IOHIDElementGetUsagePage(element);
  uint16_t usage=IOHIDElementGetUsage(element);

  IOHIDValueRef value=0;
  int v=0;
  if (IOHIDDeviceGetValue(dev->obj,element,&value)==kIOReturnSuccess) {
    v=IOHIDValueGetIntegerValue(value);
    if (v<lo) v=lo; else if (v>hi) v=hi;
  }

  ps_machid_dev_define_button(dev,cookie,(usagepage<<16)|usage,lo,hi,v);

  return 0;
}

static int ps_machid_dev_handshake(struct ps_machid_dev *dev,int vid,int pid,int usagepage,int usage) {

  dev->vid=vid;
  dev->pid=pid;
  dev->usagepage=usagepage;
  dev->usage=usage;

  /* Store manufacturer name, product name, and serial number if we can get them. */
  dev->mfrname=ps_machid_dev_get_string(dev,CFSTR(kIOHIDManufacturerKey));
  dev->prodname=ps_machid_dev_get_string(dev,CFSTR(kIOHIDProductKey));
  dev->serial=ps_machid_dev_get_string(dev,CFSTR(kIOHIDSerialNumberKey));

  /* Get limits and current value for each reported element. */
  CFArrayRef elements=IOHIDDeviceCopyMatchingElements(dev->obj,0,0);
  if (elements) {
    CFTypeID elemtypeid=IOHIDElementGetTypeID();
    CFIndex elemc=CFArrayGetCount(elements);
    CFIndex elemi; for (elemi=0;elemi<elemc;elemi++) {
      IOHIDElementRef element=(IOHIDElementRef)CFArrayGetValueAtIndex(elements,elemi);
      if (element&&(CFGetTypeID(element)==elemtypeid)) {
        ps_machid_dev_apply_IOHIDElement(dev,element);
      }
    }
    CFRelease(elements);
  }

  if (ps_machid_dev_assign_aux_btnid(dev)<0) return -1;

  return 0;
}

/* Connect device, callback from IOHIDManager.
 */

static void ps_machid_cb_DeviceMatching(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  
  int vid=dev_get_int(obj,CFSTR(kIOHIDVendorIDKey));
  int pid=dev_get_int(obj,CFSTR(kIOHIDProductIDKey));
  int usagepage=dev_get_int(obj,CFSTR(kIOHIDPrimaryUsagePageKey));
  int usage=dev_get_int(obj,CFSTR(kIOHIDPrimaryUsageKey));

  if (!ps_machid.delegate.test_device(obj,vid,pid,usagepage,usage)) {
    IOHIDDeviceClose(obj,0);
    return;
  }
        
  int devid=ps_machid_devid_unused();
  int p=ps_machid_dev_search_obj(obj);
  if (p>=0) return; // PANIC! Device is already listed.
  p=-p-1;
  if (ps_machid_dev_insert(p,obj,devid)<0) return;
  if (ps_machid_dev_handshake(ps_machid.devv+p,vid,pid,usagepage,usage)<0) { ps_machid_dev_remove(p); return; }
  int err=ps_machid.delegate.connect(devid);
  if (err<0) ps_machid.error=err;
}

/* Disconnect device, callback from IOHIDManager.
 */

static void ps_machid_cb_DeviceRemoval(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  int p=ps_machid_dev_search_obj(obj);
  if (p>=0) {
    int err=ps_machid.delegate.disconnect(ps_machid.devv[p].devid);
    if (err<0) ps_machid.error=err;
    ps_machid_dev_remove(p);
  }
}

/* Event callback from IOHIDManager.
 */

static void ps_machid_axis_values_from_hat(int *x,int *y,int v) {
  *x=*y=0;
  switch (v) {
    case 0: *y=-1; break;
    case 1: *x=1; *y=-1; break;
    case 2: *x=1; break;
    case 3: *x=1; *y=1; break;
    case 4: *y=1; break;
    case 5: *x=-1; *y=1; break;
    case 6: *x=-1; break;
    case 7: *x=-1; *y=-1; break;
  }
}

static void ps_machid_cb_InputValue(void *context,IOReturn result,void *sender,IOHIDValueRef value) {

  /* Locate device and buttons. */
  IOHIDElementRef element=IOHIDValueGetElement(value);
  int btnid=IOHIDElementGetCookie(element);
  if (btnid<0) return;
  IOHIDDeviceRef obj=IOHIDElementGetDevice(element);
  if (!obj) return;
  int p=ps_machid_dev_search_obj(obj);
  if (p<0) return;
  struct ps_machid_dev *dev=ps_machid.devv+p;
  int btnp=ps_machid_dev_search_button(dev,btnid);
  if (btnp<0) return;
  struct ps_machid_btn *btn=dev->btnv+btnp;

  /* Clamp value and confirm it actually changed. */
  CFIndex v=IOHIDValueGetIntegerValue(value);
  if (v<btn->lo) v=btn->lo;
  else if (v>btn->hi) v=btn->hi;
  if (v==btn->value) return;
  int ov=btn->value;
  btn->value=v;

  /* If this is a hat switch, split into two axes. */
  if (btn->btnid_aux) {
    int ovx,ovy,nvx,nvy;
    ps_machid_axis_values_from_hat(&ovx,&ovy,ov);
    ps_machid_axis_values_from_hat(&nvx,&nvy,v);
    if (ovx!=nvx) {
      int err=ps_machid.delegate.button(dev->devid,btnid,nvx);
      if (err<0) ps_machid.error=err;
    }
    if (ovy!=nvy) {
      int err=ps_machid.delegate.button(dev->devid,btn->btnid_aux,nvy);
      if (err<0) ps_machid.error=err;
    }
    return;
  }

  /* Report ordinary scalar buttons. */
  int err=ps_machid.delegate.button(dev->devid,btnid,v);
  if (err<0) ps_machid.error=err;
}

/* Default delegate callbacks.
 */
 
static int ps_machid_delegate_test_device_default(void *hiddevice,int vid,int pid,int usagepage,int usage) {
  if (usagepage==0x0001) switch (usage) { // Usage Page 0x0001: Generic Desktop Controls
    case 0x0004: return 1; // ...0x0004: Joystick
    case 0x0005: return 1; // ...0x0005: Game Pad
    case 0x0008: return 1; // ...0x0008: Multi-axis Controller
  }
  return 0;
}

static int ps_machid_delegate_connect_default(int devid) {
  return 0;
}

static int ps_machid_delegate_disconnect_default(int devid) {
  return 0;
}

static int ps_machid_delegate_button_default(int devid,int btnid,int value) {
  return 0;
}

/* Init.
 */

int ps_machid_init(const struct ps_machid_delegate *delegate) {
  if (ps_machid.hidmgr) return -1; // Already initialized.
  memset(&ps_machid,0,sizeof(ps_machid));
  
  if (!(ps_machid.hidmgr=IOHIDManagerCreate(kCFAllocatorDefault,0))) return -1;

  if (delegate) memcpy(&ps_machid.delegate,delegate,sizeof(struct ps_machid_delegate));
  if (!ps_machid.delegate.test_device) ps_machid.delegate.test_device=ps_machid_delegate_test_device_default;
  if (!ps_machid.delegate.connect) ps_machid.delegate.connect=ps_machid_delegate_connect_default;
  if (!ps_machid.delegate.disconnect) ps_machid.delegate.disconnect=ps_machid_delegate_disconnect_default;
  if (!ps_machid.delegate.button) ps_machid.delegate.button=ps_machid_delegate_button_default;

  IOHIDManagerRegisterDeviceMatchingCallback(ps_machid.hidmgr,ps_machid_cb_DeviceMatching,0);
  IOHIDManagerRegisterDeviceRemovalCallback(ps_machid.hidmgr,ps_machid_cb_DeviceRemoval,0);
  IOHIDManagerRegisterInputValueCallback(ps_machid.hidmgr,ps_machid_cb_InputValue,0);

  IOHIDManagerSetDeviceMatching(ps_machid.hidmgr,0); // match every HID

  IOHIDManagerScheduleWithRunLoop(ps_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
    
  if (IOHIDManagerOpen(ps_machid.hidmgr,0)<0) {
    IOHIDManagerUnscheduleFromRunLoop(ps_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
    IOHIDManagerClose(ps_machid.hidmgr,0);
    memset(&ps_machid,0,sizeof(ps_machid));
    return -1;
  }
  
  return 0;
}

/* Quit.
 */

void ps_machid_quit() {
  if (!ps_machid.hidmgr) return;
  IOHIDManagerUnscheduleFromRunLoop(ps_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
  IOHIDManagerClose(ps_machid.hidmgr,0);
  if (ps_machid.devv) {
    while (ps_machid.devc--) ps_machid_dev_cleanup(ps_machid.devv+ps_machid.devc);
    free(ps_machid.devv);
  }
  memset(&ps_machid,0,sizeof(ps_machid));
}

/* Update.
 */

int ps_machid_update(double timeout) {
  if (!ps_machid.hidmgr) return -1;
  CFRunLoopRunInMode(AKMACHID_RUNLOOP_MODE,timeout,0);
  if (ps_machid.error) {
    int result=ps_machid.error;
    ps_machid.error=0;
    return result;
  }
  return 0;
}

/* Trivial public device accessors.
 */

int ps_machid_count_devices() {
  return ps_machid.devc;
}

int ps_machid_devid_for_index(int index) {
  if ((index<0)||(index>=ps_machid.devc)) return 0;
  return ps_machid.devv[index].devid;
}

int ps_machid_index_for_devid(int devid) {
  return ps_machid_dev_search_devid(devid);
}

#define GETINT(fldname) { \
  int p=ps_machid_dev_search_devid(devid); \
  if (p<0) return 0; \
  return ps_machid.devv[p].fldname; \
}
#define GETSTR(fldname) { \
  int p=ps_machid_dev_search_devid(devid); \
  if (p<0) return ""; \
  if (!ps_machid.devv[p].fldname) return ""; \
  return ps_machid.devv[p].fldname; \
}
 
void *ps_machid_dev_get_IOHIDDeviceRef(int devid) GETINT(obj)
int ps_machid_dev_get_vendor_id(int devid) GETINT(vid)
int ps_machid_dev_get_product_id(int devid) GETINT(pid)
int ps_machid_dev_get_usage_page(int devid) GETINT(usagepage)
int ps_machid_dev_get_usage(int devid) GETINT(usage)
const char *ps_machid_dev_get_manufacturer_name(int devid) GETSTR(mfrname)
const char *ps_machid_dev_get_product_name(int devid) GETSTR(prodname)
const char *ps_machid_dev_get_serial_number(int devid) GETSTR(serial)

#undef GETINT
#undef GETSTR

/* Count buttons.
 * We need to report the 'aux' as distinct buttons.
 */

int ps_machid_dev_count_buttons(int devid) {
  int devp=ps_machid_dev_search_devid(devid);
  if (devp<0) return 0;
  const struct ps_machid_dev *dev=ps_machid.devv+devp;
  int btnc=dev->btnc;
  const struct ps_machid_btn *btn=dev->btnv;
  int i=dev->btnc; for (;i-->0;btn++) {
    if (btn->btnid_aux) btnc++;
  }
  return btnc;
}

/* Get button properties.
 */

int ps_machid_dev_get_button_info(int *btnid,int *usage,int *lo,int *hi,int *value,int devid,int index) {
  if (index<0) return -1;
  int devp=ps_machid_dev_search_devid(devid);
  if (devp<0) return -1;
  struct ps_machid_dev *dev=ps_machid.devv+devp;

  /* Scalar or hat X, the principal button. */
  if (index<dev->btnc) {
    struct ps_machid_btn *btn=dev->btnv+index;
    if (btnid) *btnid=btn->btnid;
    if (usage) *usage=btn->usage;
    if (btn->btnid_aux) {
      if (lo) *lo=-1;
      if (hi) *hi=1;
      if (value) switch (btn->value) {
        case 1: case 2: case 3: *value=1; break;
        case 5: case 6: case 7: *value=-1; break;
        default: *value=0;
      }
    } else {
      if (lo) *lo=btn->lo;
      if (hi) *hi=btn->hi;
      if (value) *value=btn->value;
    }
    return 0;
  }

  /* Is this an 'aux' button? */
  index-=dev->btnc;
  const struct ps_machid_btn *btn=dev->btnv;
  int i=dev->btnc; for (;i-->0;btn++) {
    if (!btn->btnid_aux) continue;
    if (!index--) {
      if (btnid) *btnid=btn->btnid_aux;
      if (usage) *usage=btn->usage;
      if (lo) *lo=-1;
      if (hi) *hi=1;
      if (value) switch (btn->value) {
        case 7: case 0: case 1: *value=-1; break;
        case 3: case 4: case 5: *value=1; break;
        default: *value=0;
      }
      return 0;
    }
  }

  return -1;
}
