#include "ma_macos_internal.h"

/* Globals.
 */

/* Quit.
 */

static void ma_macos_quit() {
}

/* Init.
 */

static int ma_macos_init() {
  setup();
  return 0;
}

/* Update.
 */

static int ma_macos_update() {
  loop();
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  struct ma_macioc_delegate delegate={
    .quit=ma_macos_quit,
    .init=ma_macos_init,
    .update=ma_macos_update,
  };
  return ma_macioc_main(argc,argv,&delegate);
}
