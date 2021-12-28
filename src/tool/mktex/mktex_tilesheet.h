#ifndef MKTEX_TILESHEET_H
#define MKTEX_TILESHEET_H

#include <stdint.h>

struct png_image;
struct encoder;

struct mktex_tilesheet {
  struct mktex_tile {
    uint8_t tileid;
    uint8_t ctab8[4];
    uint16_t ctab16[4];
    uint8_t colorc; // 1..4, counts the transparent dummy if present
    uint8_t flags; // MA_TEXTURE_TRANSPARENCY
    uint8_t xform;
    uint8_t w,h; // 1..255, (w) multiple of 4
    uint8_t vborrow; // if (!v) use other tile's (v)
    void *v; // (w*h)>>2; OPTIONAL
  } *tilev;
  int tilec,tilea;
  int tilecontigc;
  int singleton; // Nonzero if we're a single image, not a tilesheet
  char *name;
  int namec;
};

void mktex_tilesheet_cleanup(struct mktex_tilesheet *tilesheet);

/* Split the image into tiles if required, then enumerate and convert them.
 * (image) must be 32-bit RGBA.
 * (tilesize):
 *  - zero for a single image
 *  - <0 for auto-measure (tiles must have a fully transparent border).
 *  - otherwise it must be a multiple of 4 in 4..252.
 * This may overwrite (image)'s pixels.
 */
int mktex_tilesheet_read_image(
  struct mktex_tilesheet *tilesheet,
  struct png_image *image,
  const char *name,int namec,
  int tilesize
);

/* Look for duplication we can exploit, eg two tiles that vary only by ctab or xform.
 */
int mktex_tilesheet_optimize(
  struct mktex_tilesheet *tilesheet
);

/* Generate the C text for a tilesheet.
 * This is fairly dumb, unlikely to fail.
 */
int mktex_tilesheet_encode(
  struct encoder *encoder,
  struct mktex_tilesheet *tilesheet
);

#endif
