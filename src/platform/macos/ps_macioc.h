/* ps_macioc.h
 */

#ifndef PS_MACIOC_H
#define PS_MACIOC_H

// Must set nonzero during ma_init().
extern int ma_macioc_micros_per_frame;

struct ma_macioc_delegate {
  void (*quit)();
  int (*init)();
  int (*update)();
};

int ma_macioc_main(int argc,char **argv,const struct ma_macioc_delegate *delegate);
void ma_macioc_quit();
void ma_macioc_abort(const char *fmt,...);

#endif
