#ifndef PS_MACIOC_INTERNAL_H
#define PS_MACIOC_INTERNAL_H

#include "ps_macioc.h"
#include "multiarcade.h"
#include <Cocoa/Cocoa.h>

@interface AKAppDelegate : NSObject <NSApplicationDelegate> {
}
@end

extern struct ps_macioc {
  int init;
  struct ma_macioc_delegate delegate;
  int terminate;
  int focus;
  int update_in_progress;
} ps_macioc;

int ps_macioc_call_init();
void ps_macioc_call_quit();

#endif
