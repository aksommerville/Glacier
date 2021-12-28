

/* XXX dump color table
 */
 
#include <fcntl.h>
#include <unistd.h>
 
static void dump_ctab() {
  const char *path="etc/tapalette.rgb";
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) {
    fprintf(stderr,"%s: open failed\n",path);
    return;
  }
  if (write(fd,ma_ctab8,768)!=768) {
    fprintf(stderr,"%s: write failed\n",path);
    close(fd);
    unlink(path);
    return;
  }
  close(fd);
  fprintf(stderr,"%s: wrote color table\n",path);
}
